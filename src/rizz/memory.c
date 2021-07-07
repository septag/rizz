#include "internal.h"

#include "sx/array.h"
#include "sx/string.h"
#include "sx/hash.h"
#include "sx/os.h"
#include "sx/threads.h"
#include "sx/pool.h"

#include "rizz/imgui.h"
#include "rizz/imgui-extra.h"

static void mem_callstack_error_msg(const char* msg, ...);

#if SX_PLATFORM_WINDOWS
#    define SW_IMPL
#    define SW_ASSERT(e) sx_assert(e)
#    define SW_MAX_FRAMES 20
#    define SW_MAX_NAME_LEN 256
#    define SW_LOG_ERROR(fmt, ...) mem_callstack_error_msg(fmt, ##__VA_ARGS__)
#endif
#include "stackwalkerc/stackwalkerc.h"

#include <stdlib.h>  // malloc,free,realloc
#include <float.h>

#include "c89atomic/c89atomic.h"

#define mem_trace_context_mutex_enter(opts, mtx) if (opts&RIZZ_MEMOPTION_MULTITHREAD) sx_mutex_enter(&mtx)
#define mem_trace_context_mutex_exit(opts, mtx)  if (opts&RIZZ_MEMOPTION_MULTITHREAD) sx_mutex_exit(&mtx)


// feature-list:
// callstack
// leaks
// canaries (with runtime checks)
// serializable to disk
// imgui
// recording of all calls (begin/end) to json
// peak

typedef enum mem_action
{
    MEM_ACTION_MALLOC = 0,
    MEM_ACTION_FREE,
    MEM_ACTION_REALLOC
} mem_action;

typedef struct mem_item 
{
    mem_trace_context*  owner;
    uint16_t            num_callstack_items;   // = 0, then try to read 'source_file' and 'source_func'
    mem_action          action;
    size_t              size;
    void*               ptr;

    union {
        void*           callstack[SW_MAX_FRAMES];

        struct {
            char        source_file[128];
            char        source_func[32];
            uint32_t    source_line;
        };
    };
    
    struct mem_item*    next;
    struct mem_item*    prev;
    bool                freed;         // for mallocs, reallocs that got freed, but we want to keep the record
} mem_item;

typedef struct mem_trace_context
{
    char      name[32];
    sx_alloc  my_alloc;         // all allocations are redirected from this to redirect_alloc
    sx_alloc  redirect_alloc;   // receives all alloc calls and performs main allocations
    uint32_t  name_hash;
    uint32_t  options;
    sx_align_decl(SX_CACHE_LINE_SIZE, c89atomic_int32) num_items;
    sx_align_decl(SX_CACHE_LINE_SIZE, c89atomic_int64) alloc_size;
    sx_align_decl(SX_CACHE_LINE_SIZE, c89atomic_int64) peak_size;
    sx_pool*  item_pool;    // item_size = sizeof(mem_item)
    mem_item* items_list;   // first node
    sx_mutex  mtx;

    struct mem_trace_context* parent;
    struct mem_trace_context* child;
    struct mem_trace_context* next;
    struct mem_trace_context* prev;
} mem_trace_context;

typedef struct mem_state
{
    #if SX_PLATFORM_WINDOWS
        sw_context* sw;
    #endif

    sx_align_decl(SX_CACHE_LINE_SIZE, c89atomic_int64) debug_mem_size;
    mem_trace_context* root;
} mem_state;


static mem_state g_mem;

static void mem_callstack_load_module(const char* img, const char* module, uint64_t base_addr, uint32_t size, void* userptr)
{
    sx_unused(img);
    sx_unused(base_addr);
    sx_unused(size);
    sx_unused(userptr);

    rizz__log_debug("(init) module: %s (size=%$d)", module, size);
}

static mem_trace_context* mem_find_trace_context(uint32_t name_hash, mem_trace_context* node)
{
    if (node->name_hash == name_hash) {
        return node;
    }

    mem_trace_context* child = node->child;
    while (child) {
        mem_trace_context* found = mem_find_trace_context(name_hash, child);
        if (found) {
            return found;
        }
        child = child->next;
    }

    return NULL;
}

static void mem_callstack_error_msg(const char* msg, ...)
{
    sx_unused(msg);
    
    #if SX_PLATFORM_WINDOWS
        char formatted[512];
        va_list args;
        va_start(args, msg);
        sx_vsnprintf(formatted, sizeof(formatted), msg, args);
        va_end(args);
        sx_strcat(formatted, sizeof(formatted), "\n");
        OutputDebugStringA(formatted);
    #endif
}

static mem_trace_context* mem_create_trace_context(const char* name, uint32_t mem_opts, const char* parent)
{
    sx_assert(name);
    uint32_t name_hash = sx_hash_fnv32_str(name);
    if (g_mem.root) {
        if (mem_find_trace_context(name_hash, g_mem.root)) {
            sx_assert_alwaysf(0, "duplicate name exists for mem_trace_context: %s", name);
            return NULL;
        }
    }

    mem_trace_context* parent_ctx = g_mem.root;
    if (parent && parent[0]) {
        parent_ctx = mem_find_trace_context(sx_hash_fnv32_str(parent), g_mem.root);
        if (!parent_ctx) {
            sx_assert_alwaysf(0, "parent does not exist for mem_trace_context: %s, parent: %s", name, parent);
            return NULL;
        }
    }

    mem_trace_context* ctx = sx_calloc(sx_alloc_malloc(), sizeof(mem_trace_context));
    if (!ctx) {
        sx_memory_fail();
        return NULL;
    }

    sx_strcpy(ctx->name, sizeof(ctx->name), name);
    ctx->name_hash = name_hash;
    ctx->options = (mem_opts == RIZZ_MEMOPTION_INHERIT && parent_ctx) ? parent_ctx->options : mem_opts;
    ctx->parent = parent_ctx;
    ctx->item_pool = sx_pool_create(sx_alloc_malloc(), sizeof(mem_item), 150);
    if (!ctx->item_pool) {
        sx_free(sx_alloc_malloc(), ctx);        
        sx_memory_fail();
        return NULL;
    }

    if (parent_ctx) {
        if (parent_ctx->child) {
            parent_ctx->child->prev = ctx;
            ctx->next = parent_ctx->child;
        }
        parent_ctx->child = ctx;
    }

    if (ctx->options & RIZZ_MEMOPTION_MULTITHREAD) {
        sx_mutex_init(&ctx->mtx);
    }

    return ctx;
}

static mem_item* mem_find_occupied_item(mem_trace_context* ctx, void* ptr)
{
    mem_item* it = ctx->items_list;
    while (it) {
        if (it->ptr == ptr && !it->freed) {
            return it;
        }
        it = it->next;
    }

    return NULL;
}


static void mem_destroy_trace_item(mem_trace_context* ctx, mem_item* item)
{
    sx_assert(ctx);

    // unlink
    mem_trace_context_mutex_enter(ctx->options, ctx->mtx);
    if (item->next) {
        item->next->prev = item->prev;
    }
    if (item->prev) {
        item->prev->next = item->next;
    }
    if (ctx->items_list == item) {
        ctx->items_list = item->next;
    }
    sx_pool_del(ctx->item_pool, item);
    mem_trace_context_mutex_exit(ctx->options, ctx->mtx);
}

static void mem_create_trace_item(mem_trace_context* ctx, void* ptr, void* old_ptr, 
                                  size_t size, const char* file, const char* func, uint32_t line)
{
    mem_item item = {0};
    if (old_ptr) {
        item.action = (size == 0) ? MEM_ACTION_FREE : MEM_ACTION_REALLOC;
    }
    bool save_current_call = item.action != MEM_ACTION_FREE;

    if (save_current_call) {
        c89atomic_fetch_add_explicit_i64(&g_mem.debug_mem_size, sizeof(mem_item), c89atomic_memory_order_relaxed);

        if (ctx->options & RIZZ_MEMOPTION_TRACE_CALLSTACK) {
            #if SX_PLATFORM_WINDOWS
                item.num_callstack_items = sw_capture_current(g_mem.sw, item.callstack);
                if (item.num_callstack_items == 0) {
                    if (file)  sx_strcpy(item.source_file, sizeof(item.source_file), file);
                    if (func)  sx_strcpy(item.source_func, sizeof(item.source_func), func);
                    item.source_line = line;
                } 
            #else
                if (file)  sx_strcpy(item.source_file, sizeof(item.source_file), file);
                if (func)  sx_strcpy(item.source_func, sizeof(item.source_func), func);
                item.source_line = line;
            #endif
        }
    }

    item.owner = ctx;
    item.size = size;
    item.ptr = ptr;

    // propogate memory usage to the context and it's parents
    if (item.action == MEM_ACTION_FREE || item.action == MEM_ACTION_REALLOC) {
        mem_trace_context_mutex_enter(ctx->options, ctx->mtx);
        mem_item* old_item = mem_find_occupied_item(ctx, old_ptr);
        mem_trace_context_mutex_exit(ctx->options, ctx->mtx);

        // old_ptr should be either malloc or realloc and belong to this mem_trace_context
        sx_assert(old_item && old_item->action == MEM_ACTION_MALLOC || old_item->action == MEM_ACTION_REALLOC);
        if (old_item) {
            old_item->freed = true;
            sx_assert(old_item->size > 0);
            mem_trace_context* _ctx = ctx;
            while (_ctx) {
                c89atomic_fetch_add_explicit_i32(&_ctx->num_items, -1, c89atomic_memory_order_relaxed);                
                c89atomic_fetch_add_explicit_i64(&_ctx->alloc_size, -(int64_t)old_item->size, c89atomic_memory_order_relaxed);
                _ctx = _ctx->parent;
            }

            if (item.action == MEM_ACTION_FREE) {
                item.size = old_item->size;
            }

            mem_destroy_trace_item(ctx, old_item);
        }
    } 

    if (size > 0) {
        mem_trace_context* _ctx = ctx;
        while (_ctx) {
            c89atomic_fetch_add_explicit_i32(&_ctx->num_items, 1, c89atomic_memory_order_relaxed);
            c89atomic_fetch_add_explicit_i64(&_ctx->alloc_size, (int64_t)size, c89atomic_memory_order_acquire);
            
            int64_t cur_peak = _ctx->peak_size;
            while (cur_peak < ctx->alloc_size && 
                   !c89atomic_compare_exchange_strong_i64(&_ctx->peak_size, &cur_peak, _ctx->alloc_size))
            { 
                sx_yield_cpu(); 
            }
            

            _ctx = _ctx->parent;
        }
    } 

    // create tracking item and add to linked-list
    if (save_current_call) {
        mem_trace_context_mutex_enter(ctx->options, ctx->mtx);
        mem_item* new_item = (mem_item*)sx_pool_new_and_grow(ctx->item_pool, sx_alloc_malloc());
        if (!new_item) {
            mem_trace_context_mutex_exit(ctx->options, ctx->mtx);
            sx_memory_fail();
            return;
        }
        memcpy(new_item, &item, sizeof(item));

        if (ctx->items_list) {
            ctx->items_list->prev = new_item;
            new_item->next = ctx->items_list;
        } 
        ctx->items_list = new_item;
        mem_trace_context_mutex_exit(ctx->options, ctx->mtx);
    }
}

static void mem_destroy_trace_context(mem_trace_context* ctx)
{
    if (ctx) {
        if (g_mem.root && !mem_find_trace_context(sx_hash_fnv32_str(ctx->name), g_mem.root)) {
            sx_assert_alwaysf(0, "mem_trace_context already destroyed/invalid: %s", ctx->name);
            return;
        }

        // destroy children recursively
        if (ctx->child) {
            mem_trace_context* child = ctx->child;
            while (child) {
                mem_trace_context* next = child->next;
                mem_destroy_trace_context(child);
                child = next;
            }
        }

        // unlink
        if (ctx->parent) {
            mem_trace_context* parent = ctx->parent;
            if (parent->child == ctx) {
                parent->child = ctx->next;
            }
        }

        if (ctx->next) {
            ctx->next->prev = ctx->prev;
        }

        if (ctx->prev) {
            ctx->prev->next = ctx->next;
        }

        if (ctx->options & RIZZ_MEMOPTION_MULTITHREAD) {
            sx_mutex_release(&ctx->mtx);
        }

        sx_pool_destroy(ctx->item_pool, sx_alloc_malloc());
        sx_free(sx_alloc_malloc(), ctx);
    }
}

void rizz__mem_destroy_allocator(sx_alloc* alloc)
{
    if (alloc) {
        mem_trace_context* ctx = alloc->user_data;
        mem_destroy_trace_context(ctx);
    }
}

static void mem_trace_context_clear(mem_trace_context* ctx)
{
    mem_trace_context_mutex_enter(ctx->options, ctx->mtx);
    mem_item* item = ctx->items_list;
    while (item) {
        mem_item* next = item->next;
        sx_pool_del(ctx->item_pool, item);
        item = next;
    }

    ctx->items_list = NULL;
    mem_trace_context_mutex_exit(ctx->options, ctx->mtx);
}

static void* mem_alloc_cb(void* ptr, size_t size, uint32_t align, const char* file, const char* func, 
                          uint32_t line, void* user_data)
{
    mem_trace_context* ctx = user_data;
    void* p = ctx->redirect_alloc.alloc_cb(ptr, size, align, file, func, line, user_data);
    mem_create_trace_item(ctx, p, ptr, size, file, func, line);
    return p;
}

bool rizz__mem_init(uint32_t opts)
{
    sx_assert(opts != RIZZ_MEMOPTION_INHERIT);

    #if SX_PLATFORM_WINDOWS
        g_mem.sw = sw_create_context_capture(
            SW_OPTIONS_SYMBOL|SW_OPTIONS_SOURCEPOS|SW_OPTIONS_MODULEINFO|SW_OPTIONS_SYMBUILDPATH,
            (sw_callbacks) { .load_module = mem_callstack_load_module }, NULL);
        if (!g_mem.sw) {
            return false;
        }

        char vspath[SW_MAX_NAME_LEN];
        if (rizz__win_get_vstudio_dir(vspath, sizeof(vspath))) {
            #if SX_ARCH_64BIT
                sx_strcat(vspath, sizeof(vspath), "Common7\\IDE\\Extensions\\TestPlatform\\Extensions\\Cpp\\x64");
            #elif SX_ARCH_32BIT
                sx_strcat(vspath, sizeof(vspath), "Common7\\IDE\\Extensions\\TestPlatform\\Extensions\\Cpp");
            #endif
            sw_set_dbghelp_hintpath(vspath);
        }

        sw_set_callstack_limits(g_mem.sw, 3, SW_MAX_FRAMES);
    #endif // SX_PLATFORM_WINDOWS

    // dummy root trace context
    g_mem.root = mem_create_trace_context("<memory>", opts, NULL);
    if (!g_mem.root) {
        sx_memory_fail();
        return false;
    }

    return true;
}

void rizz__mem_release(void)
{
    #if SX_PLATFORM_WINDOWS
        sw_destroy_context(g_mem.sw);
    #endif

    if (g_mem.root) {
        mem_destroy_trace_context(g_mem.root);
    }
}

sx_alloc* rizz__mem_create_allocator(const char* name, uint32_t mem_opts, const char* parent, const sx_alloc* alloc)
{
    sx_assert(alloc);
    mem_trace_context* ctx = mem_create_trace_context(name, mem_opts, parent);

    if (ctx) {
        ctx->my_alloc.alloc_cb = mem_alloc_cb;
        ctx->my_alloc.user_data = ctx;
        ctx->redirect_alloc = *alloc;
        
        return &ctx->my_alloc;
    }

    return NULL;
}

void rizz__mem_allocator_clear_trace(sx_alloc* alloc)
{
    mem_trace_context* ctx = alloc->user_data;
    mem_trace_context_clear(ctx);
}

static void mem_trace_dump_entry(mem_item* item, int depth, sw_callstack_entry entries[SW_MAX_FRAMES])
{
    sx_assert(depth < 256);
    if (item->action == MEM_ACTION_FREE) {
        return;
    }
    char depth_str[256] = {0};
    for (int i = 0; i < depth; i++) {
        depth_str[i] = '\t';
    }
    depth_str[depth] = '\0';

    const char* action = "";
    switch (item->action) {
        case MEM_ACTION_MALLOC:  action = "malloc";  break;
        case MEM_ACTION_FREE:    action = "Free";    break;
        case MEM_ACTION_REALLOC: action = "Realloc"; break;
    }
    rizz__log_debug("%s%s: %p (%llu)", depth_str, action, item->ptr, item->size);

    if (item->num_callstack_items != 0) {
        #if SX_PLATFORM_WINDOWS
            uint16_t num_resolved = sw_resolve_callstack(g_mem.sw, item->callstack, entries, item->num_callstack_items);
            if (num_resolved) {
                rizz__log_debug("%sCallstack:", depth_str);
                for (int i = 0; i < num_resolved; i++) {
                    rizz__log_debug("%s%s(%u) - %s", depth_str, entries[i].line_filename, entries[i].line, entries[i].name);
                }
            } 
        #endif
    } else {
        rizz__log_debug("%s%s(%u) - %s", depth_str, item->source_file, item->source_line, item->source_func);
    }
}

typedef struct mem_imgui_state
{
    mem_trace_context* selected_ctx;
    mem_item* selected_item;
    int selected_stack_item;
    int dummy;
} mem_imgui_state;

static mem_imgui_state g_mem_imgui;

static void mem_imgui_context_node(rizz_api_imgui* imgui, mem_trace_context* ctx)
{
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow|ImGuiTreeNodeFlags_SpanFullWidth|
                               ImGuiTreeNodeFlags_OpenOnDoubleClick;
    if (!ctx->child) {
        flags |= ImGuiTreeNodeFlags_Leaf;
    }

    if (ctx == g_mem_imgui.selected_ctx) {
        flags |= ImGuiTreeNodeFlags_Selected;
    }

    if (imgui->TreeNodeEx_Str(ctx->name, flags)) {
        if (imgui->IsItemClicked(ImGuiMouseButton_Left)) {
            g_mem_imgui.selected_ctx = ctx;
            g_mem_imgui.selected_item = NULL;
            g_mem_imgui.selected_stack_item = -1;
        }

        ctx = ctx->child;
        while (ctx) {
            mem_imgui_context_node(imgui, ctx);
            ctx = ctx->next;
        }

        imgui->TreePop();
    }
}

static void mem_imgui_context_info(rizz_api_imgui* imgui, rizz_api_imgui_extra* imguix, mem_trace_context* ctx)
{
    sx_unused(imgui);

    imguix->label("Allocated size", "%$d", ctx->alloc_size);
    imguix->label("Peak size ", "%$d", ctx->peak_size);

    char size_text[32];
    char peak_text[32];
    sx_snprintf(size_text, sizeof(size_text), "%$.2d", ctx->alloc_size);
    sx_snprintf(peak_text, sizeof(peak_text), "%$.2d", ctx->peak_size);
    const sx_vec2 progress_size = sx_vec2f(-1.0f, 14.0f);

    imguix->dual_progress_bar((float)((double)ctx->alloc_size / (double)ctx->peak_size), 1.0f,
                              progress_size, size_text, peak_text);

    imguix->label("Allocations", "%u", ctx->num_items);
    {
        int num_pool_pages = 0;
        sx__pool_page* page = ctx->item_pool->pages;
        while (page) {
            num_pool_pages++;
            page = page->next;
        }
        imguix->label("Trace memory size", "%$.2d", ctx->item_pool->capacity*ctx->item_pool->item_sz*num_pool_pages);
    }
    
    #if SX_PLATFORM_WINDOWS
        void* symbols[SW_MAX_FRAMES] = {(void*)ctx->redirect_alloc.alloc_cb};
        sw_callstack_entry entries[SW_MAX_FRAMES];
        if (sw_resolve_callstack(g_mem.sw, symbols, entries, 1)) {
            imgui->TextColored(*imgui->GetStyleColorVec4(ImGuiCol_TextDisabled), "Allocator:");
            imgui->Indent(0);
            imgui->Text(entries[0].name);
            imgui->Text("%s(%u)", entries[0].line_filename, entries[0].line);
            imgui->Text(entries[0].loaded_image_name);
        }
    #endif
}

static void mem_imgui_context_items(rizz_api_imgui* imgui, rizz_api_imgui_extra* imguix, mem_trace_context* ctx)
{
    sx_unused(imguix);
    if (imgui->BeginTable("MemItems", 4, 
                        ImGuiTableFlags_Resizable|ImGuiTableFlags_BordersV|ImGuiTableFlags_BordersOuterH|
                        ImGuiTableFlags_SizingFixedFit|ImGuiTableFlags_RowBg|ImGuiTableFlags_ScrollY,
                        SX_VEC2_ZERO, 0)) {
        char text[32];
        ImGuiListClipper clipper;

        // gather all items
        mem_item** items = NULL;    // sx_array
        rizz__with_temp_alloc(tmp_alloc) {
            {
                mem_trace_context_mutex_enter(ctx->options, ctx->mtx);
                mem_item* item = ctx->items_list;
                while (item) {
                    sx_array_push(tmp_alloc, items, item);
                    item = item->next;
                }
                mem_trace_context_mutex_exit(ctx->options, ctx->mtx);
            }
            int num_items = sx_array_count(items);

            if (num_items) {
                imgui->TableSetupColumn("#", 0, 35.0f, 0);
                imgui->TableSetupColumn("Ptr", 0, 150.0f, 0);
                imgui->TableSetupColumn("Size", 0, 50.0f, 0);
                imgui->TableSetupColumn("Action", 0, 50.0f, 0);
                imgui->TableHeadersRow();

                imgui->ImGuiListClipper_Begin(&clipper, num_items, -1.0f);
                while (imgui->ImGuiListClipper_Step(&clipper)) {
                    for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
                        imgui->TableNextRow(0, 0);
                        mem_item* item = items[i];

                        sx_snprintf(text, sizeof(text), "%d", i + 1);
                        imgui->TableNextColumn();
                        imgui->PushID_Int(i);
                        if (imgui->Selectable_Bool(text, g_mem_imgui.selected_item == item,
                                                   ImGuiSelectableFlags_SpanAllColumns, SX_VEC2_ZERO)) {
                            g_mem_imgui.selected_item = item;
                        }

                        if (imgui->BeginPopupContextItem("MemItemContextMenu", ImGuiPopupFlags_MouseButtonRight)) {
                            if (imgui->Selectable_Bool("Copy address", false, 0, SX_VEC2_ZERO)) {
                                char ptr_str[32];
                                sx_snprintf(ptr_str, sizeof(ptr_str), "%p", item->ptr);
                                the__app.set_clipboard_string(ptr_str);
                            }
                            imgui->EndPopup();
                        }
                        imgui->PopID();

                        sx_snprintf(text, sizeof(text), "0x%p", item->ptr);
                        imgui->TableNextColumn();
                        imgui->Text(text);

                        sx_snprintf(text, sizeof(text), "%$.2d", item->size);
                        imgui->TableNextColumn();
                        imgui->Text(text);

                        imgui->TableNextColumn();
                        switch (item->action) {
                        case MEM_ACTION_MALLOC:     imgui->Text("malloc");  break;
                        case MEM_ACTION_REALLOC:    imgui->Text("realloc"); break;
                        case MEM_ACTION_FREE:       imgui->Text("free"); break;
                        default:                    imgui->Text("N/A"); break;
                        }
                    }
                }
                imgui->ImGuiListClipper_End(&clipper);
            }
        }   // temp_alloc
    }
    imgui->EndTable();
}

static void mem_imgui_open_vscode_file_loc(const char* filename, uint32_t line)
{
    char vscode_path[RIZZ_MAX_PATH];
    char goto_str[RIZZ_MAX_PATH];
    sx_snprintf(goto_str, sizeof(goto_str), "\"%s:%u\"", filename, line);

    #if SX_PLATFORM_WINDOWS
        char local_dir[RIZZ_MAX_PATH];
        GetEnvironmentVariableA("LocalAppData", local_dir, sizeof(local_dir));
        sx_strcpy(vscode_path, sizeof(vscode_path), "\"");
        sx_strcat(vscode_path, sizeof(vscode_path), local_dir);
        sx_strcat(vscode_path, sizeof(vscode_path), "\\Programs\\Microsoft VS Code\\Code.exe");
        sx_strcat(vscode_path, sizeof(vscode_path), "\"");

        char vscode_cmd[512];
        sx_strcpy(vscode_cmd, sizeof(vscode_cmd), "\"");
        sx_strcat(vscode_cmd, sizeof(vscode_cmd), vscode_path);
        sx_strcat(vscode_cmd, sizeof(vscode_cmd), " --goto ");
        sx_strcat(vscode_cmd, sizeof(vscode_cmd), goto_str);
        sx_strcat(vscode_cmd, sizeof(vscode_cmd), "\"");
        const char* args[] = {"C:\\Windows\\System32\\cmd.exe", "/C", vscode_cmd, 0};
        sx_os_exec(args);
    #else
        const char* args[] = {"code", "--goto", goto_str, 0};
        sx_os_exec(args);
    #endif
}

static void mem_imgui_item(rizz_api_imgui* imgui, rizz_api_imgui_extra* imguix, mem_item* item)
{
    sx_unused(imguix);

    #if SX_PLATFORM_WINDOWS
        if (item->num_callstack_items) {
            char module_name[32];
            if (sw_get_symbol_module(g_mem.sw, item->callstack[0], module_name)) {
                imguix->label("Module", module_name);
            } else {
                imguix->label("Module", "N/A");
            }
        }
    #endif
    imgui->TextColored(*imgui->GetStyleColorVec4(ImGuiCol_TextDisabled), "Callstack:");
    imgui->Indent(0);
    char text[512];
    if (item->num_callstack_items) {
        #if SX_PLATFORM_WINDOWS
            sw_callstack_entry entries[SW_MAX_FRAMES];
            uint16_t resolved = sw_resolve_callstack(g_mem.sw, item->callstack, entries, item->num_callstack_items);
            for (uint16_t i = 0; i < resolved; i++) {
                imgui->Bullet(); 
                sx_snprintf(text, sizeof(text), "%s(%u): %s", entries[i].line_filename, entries[i].line, entries[i].name);
                if (imgui->Selectable_Bool(text, i == g_mem_imgui.selected_stack_item, ImGuiSelectableFlags_SpanAvailWidth, SX_VEC2_ZERO)) {
                    g_mem_imgui.selected_stack_item = i;
                }

                if (imgui->IsItemHovered(0) && imgui->IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                    mem_imgui_open_vscode_file_loc(entries[i].line_filename, entries[i].line);
                }
            }
        #endif
    } else {
        imgui->Bullet(); 
        sx_snprintf(text, sizeof(text), "%s(%u): %s", item->source_file, item->source_line, item->source_func);
        if (imgui->Selectable_Bool(text, g_mem_imgui.selected_stack_item == 0, ImGuiSelectableFlags_SpanAvailWidth, SX_VEC2_ZERO)) {
            g_mem_imgui.selected_stack_item = 0;
        }
        if (imgui->IsItemHovered(0) && imgui->IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
            mem_imgui_open_vscode_file_loc(item->source_file, item->source_line);
        }
    }
}

void rizz__mem_show_debugger(bool* popen)
{
    rizz_api_imgui* imgui = the__plugin.get_api_byname("imgui", 0);
    rizz_api_imgui_extra* imguix = the__plugin.get_api_byname("imgui_extra", 0);
    if (!imgui || !imguix) {
        return;
    }

    imgui->SetNextWindowSizeConstraints(sx_vec2f(550, 400), sx_vec2f(FLT_MAX, FLT_MAX), NULL, NULL);
    if (imgui->Begin("Memory Debugger", popen, 0)) {
        sx_vec2 region;
        imgui->GetContentRegionAvail(&region);
        imgui->BeginChild_Str("ContextsContainer", sx_vec2f(0, region.y*0.4f), true, 0);
        if (imgui->BeginTable("Contexts", 2, 
            ImGuiTableFlags_Resizable|ImGuiTableFlags_Borders|ImGuiTableFlags_SizingStretchProp|ImGuiTableFlags_ScrollY, 
            SX_VEC2_ZERO, 0))
        {
            imgui->TableSetupColumn("ContextTree", 0, region.x*0.6f, 0);
            imgui->TableSetupColumn("ContextInfo", 0, region.x*0.4f, 0);
            imgui->TableNextRow(0, 0);

            imgui->TableSetColumnIndex(0);

            mem_trace_context* ctx = g_mem.root->child;
            while (ctx) {
                mem_imgui_context_node(imgui, ctx);
                ctx = ctx->next;
            }

            imgui->TableSetColumnIndex(1);
            if (g_mem_imgui.selected_ctx) {
                mem_imgui_context_info(imgui, imguix, g_mem_imgui.selected_ctx);
            } else {
                imgui->Text("No context selected");
            }

            imgui->EndTable();
        }
        imgui->EndChild();

        if (g_mem_imgui.selected_ctx) {
            imgui->BeginChild_Str("ItemsContainer", SX_VEC2_ZERO, true, 0);
            if (imgui->BeginTable("Contexts", 2, 
                ImGuiTableFlags_Resizable|ImGuiTableFlags_Borders|ImGuiTableFlags_SizingStretchProp, 
                SX_VEC2_ZERO, 0))
            {
                imgui->TableSetupColumn("ContextTree", 0, region.x*0.4f, 0);
                imgui->TableSetupColumn("ContextInfo", 0, region.x*0.6f, 0);
                imgui->TableNextRow(0, 0);

                imgui->TableSetColumnIndex(0);
                mem_imgui_context_items(imgui, imguix, g_mem_imgui.selected_ctx);

                imgui->TableSetColumnIndex(1);
                if (g_mem_imgui.selected_item) {
                    mem_imgui_item(imgui, imguix, g_mem_imgui.selected_item);
                } else {
                    imgui->Text("No item selected");
                }

            }
            imgui->EndTable();
            imgui->EndChild();
        }
    }
    imgui->End();
}

void rizz__mem_reload_modules(void)
{
    #if SX_PLATFORM_WINDOWS
        sw_reload_modules(g_mem.sw);
    #endif
}