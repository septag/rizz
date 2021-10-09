#include "internal.h"

#include "sx/array.h"
#include "sx/string.h"
#include "sx/hash.h"
#include "sx/os.h"
#include "sx/threads.h"
#include "sx/pool.h"
#include "sx/io.h"
#include "sx/hash.h"

#include "rizz/imgui.h"
#include "rizz/imgui-extra.h"

#include <time.h>

static void mem_callstack_error_msg(const char* msg, ...);
char* rizz__demangle(const char* symbol);   // demangle.cpp

#if SX_PLATFORM_WINDOWS
#    define SW_IMPL
#    define SW_ASSERT(e) sx_assert(e)
#    define SW_MAX_FRAMES 20
#    define SW_MAX_NAME_LEN 256
#    define SW_LOG_ERROR(fmt, ...) mem_callstack_error_msg(fmt, ##__VA_ARGS__)
#endif
#include "stackwalkerc/stackwalkerc.h"

#include <stdlib.h>  // malloc,free,realloc,qsort
#include <float.h>
#include <string.h>  // strcmp

#include "c89atomic/c89atomic.h"

#define mem_trace_context_mutex_enter(opts, mtx) if (opts&RIZZ_MEMOPTION_MULTITHREAD) sx_mutex_enter(&mtx)
#define mem_trace_context_mutex_exit(opts, mtx)  if (opts&RIZZ_MEMOPTION_MULTITHREAD) sx_mutex_exit(&mtx)

#if SX_PLATFORM_OSX || SX_PLATFORM_LINUX
#   if SX_PLATFORM_LINUX
#   define __USE_GNU
#   endif 

#   include <execinfo.h>
#   include <dlfcn.h>
#endif

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
    
    uint32_t            callstack_hash;
    struct mem_item*    next;
    struct mem_item*    prev;
    int64_t             frame;         // record frame number
    bool                freed;         // for mallocs, reallocs that got freed, but we want to keep the record
} mem_item;

typedef struct mem_item_index
{
    int index;
    mem_item* item;
} mem_item_index;

typedef struct mem_item_collapsed
{
    int       item_idx;
    mem_item* item;
    uint32_t  count;
    char      entry_symbol[64];
    size_t    size;
} mem_item_collapsed;

typedef enum mem_item_collapsed_id
{
    MEMITEM_COLLAPSED_ORDER,
    MEMITEM_COLLAPSED_SYMBOL,
    MEMITEM_COLLAPSED_SIZE,
    MEMITEM_COLLAPSED_COUNT
} mem_item_collapsed_id;

typedef struct mem__write_json_context {
    sx_file file;
    const char* newline;
    const char* tab;

    int _depth;
    bool _is_struct_array;
    char _tabs[128];
    int _array_count;
} mem__write_json_context;

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
    mem_item_collapsed* SX_ARRAY cached;     // keep sorted cached data 

    struct mem_trace_context* parent;
    struct mem_trace_context* child;
    struct mem_trace_context* next;
    struct mem_trace_context* prev;
} mem_trace_context;

typedef struct mem_capture_context
{
    char name[32];
    uint64_t start_tm; 
    sx_mutex mtx;
    mem_item** SX_ARRAY items; 
} mem_capture_context;

typedef struct mem_state
{
    #if SX_PLATFORM_WINDOWS
        sw_context* sw;
    #endif

    const sx_alloc* alloc;
    sx_align_decl(SX_CACHE_LINE_SIZE, c89atomic_int64) debug_mem_size;
    mem_trace_context* root;
    mem_capture_context capture;
    sx_align_decl(SX_CACHE_LINE_SIZE, c89atomic_flag) in_capture;
} mem_state;

typedef struct mem_imgui_state
{
    mem_trace_context* selected_ctx;
    mem_item* selected_item;
    int selected_stack_item;
    int dummy;
    const ImGuiTableColumnSortSpecs* sort_spec;
    bool collapse_items;
} mem_imgui_state;

static mem_state g_mem;
static mem_imgui_state g_mem_imgui;

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

    mem_trace_context* ctx = sx_calloc(g_mem.alloc, sizeof(mem_trace_context));
    if (!ctx) {
        sx_memory_fail();
        return NULL;
    }

    sx_strcpy(ctx->name, sizeof(ctx->name), name);
    ctx->name_hash = name_hash;
    ctx->options = (mem_opts == RIZZ_MEMOPTION_INHERIT && parent_ctx) ? parent_ctx->options : mem_opts;
    ctx->parent = parent_ctx;
    ctx->item_pool = sx_pool_create(g_mem.alloc, sizeof(mem_item), 150);
    if (!ctx->item_pool) {
        sx_free(g_mem.alloc, ctx);        
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
    item->next = item->prev = NULL;
    item->freed = false;

    sx_pool_del(ctx->item_pool, item);

    sx_array_clear(ctx->cached);
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
                item.num_callstack_items = sw_capture_current(g_mem.sw, item.callstack, &item.callstack_hash);
                if (item.num_callstack_items == 0) {
                    if (file)  sx_strcpy(item.source_file, sizeof(item.source_file), file);
                    if (func)  sx_strcpy(item.source_func, sizeof(item.source_func), func);
                    item.source_line = line;
                    item.callstack_hash = sx_hash_xxh32(item.callstack, sizeof(item.callstack), 0);
                }
            #elif SX_PLATFORM_OSX || SX_PLATFORM_LINUX
                item.num_callstack_items = backtrace(item.callstack, SW_MAX_FRAMES);
                if (item.num_callstack_items == 0) {
                    if (file)   sx_strcpy(item.source_file, sizeof(item.source_file), file);
                    if (func)   sx_strcpy(item.source_func, sizeof(item.source_func), file);
                    item.source_line = line;
                    item.callstack_hash = sx_hash_xxh32(item.callstack, item.num_callstack_items*sizeof(void*), 0);
                } else {
                    item.callstack_hash = sx_hash_xxh32(item.callstack, sizeof(item.callstack), 0);
                }
            #else
                if (file)  sx_strcpy(item.source_file, sizeof(item.source_file), file);
                if (func)  sx_strcpy(item.source_func, sizeof(item.source_func), func);
                item.source_line = line;
                item.callstack_hash = sx_hash_xxh32(item.callstack, sizeof(item.callstack), 0);
            #endif
        }
    }

    item.owner = ctx;
    item.size = size;
    item.ptr = ptr;
    item.frame = the__core.frame_index();
    bool in_capture = c89atoimc_flag_load_explicit(&g_mem.in_capture, c89atomic_memory_order_acquire);

    // special case: FREE and REALLOC, always have previous malloc trace items that we should take care of
    if (item.action == MEM_ACTION_FREE || item.action == MEM_ACTION_REALLOC) {
        mem_trace_context_mutex_enter(ctx->options, ctx->mtx);
        mem_item* old_item = mem_find_occupied_item(ctx, old_ptr);
        mem_trace_context_mutex_exit(ctx->options, ctx->mtx);

        // old_ptr should be either malloc or realloc and belong to this mem_trace_context
        sx_assert(old_item && old_item->action == MEM_ACTION_MALLOC || old_item->action == MEM_ACTION_REALLOC);
        if (old_item) {
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

            if (!in_capture && item.action == MEM_ACTION_FREE) {
                mem_destroy_trace_item(ctx, old_item);
                old_item->freed = true;
            } else if (in_capture) {
                old_item->freed = true;
                sx_mutex_lock(g_mem.capture.mtx) {
                    sx_array_push(g_mem.alloc, g_mem.capture.items, old_item);
                }
            }
        }
    } 

    // propogate memory usage to the context and it's parents
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
        mem_item* new_item = (mem_item*)sx_pool_new_and_grow(ctx->item_pool, g_mem.alloc);
        if (!new_item) {
            mem_trace_context_mutex_exit(ctx->options, ctx->mtx);
            sx_memory_fail();
            return;
        }
        sx_memcpy(new_item, &item, sizeof(item));

        if (ctx->items_list) {
            ctx->items_list->prev = new_item;
        } 
        new_item->next = ctx->items_list;
        ctx->items_list = new_item;
        sx_array_clear(ctx->cached);
        mem_trace_context_mutex_exit(ctx->options, ctx->mtx);

        if (in_capture) {
            sx_mutex_lock(g_mem.capture.mtx) {
                sx_array_push(g_mem.alloc, g_mem.capture.items, new_item);
            }
        }
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
        
        sx_array_free(g_mem.alloc, ctx->cached);
        sx_pool_destroy(ctx->item_pool, g_mem.alloc);
        sx_free(g_mem.alloc, ctx);
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

static const char* k_hardcoded_vspaths[] = {
    "C:\\Program Files (x86)\\Microsoft Visual Studio\\2019\\Community\\Common7\\IDE\\Extensions\\TestPlatform\\Extensions\\Cpp\\x64"
};

bool rizz__mem_init(uint32_t opts)
{
    sx_assert(opts != RIZZ_MEMOPTION_INHERIT);

    g_mem.alloc = the__core.heap_alloc();

    #if SX_PLATFORM_WINDOWS
        g_mem.sw = sw_create_context_capture(
            SW_OPTIONS_SYMBOL|SW_OPTIONS_SOURCEPOS|SW_OPTIONS_MODULEINFO|SW_OPTIONS_SYMBUILDPATH,
            (sw_callbacks) { .load_module = mem_callstack_load_module }, NULL);
        if (!g_mem.sw) {
            return false;
        }

        char vspath[SW_MAX_NAME_LEN] = {0};

        size_t num_hardcoded_paths = sizeof(k_hardcoded_vspaths) / sizeof(char*);
        for (size_t i = 0; i < num_hardcoded_paths; i++) {
            if (sx_os_path_isdir(k_hardcoded_vspaths[i])) {
                sx_strcpy(vspath, sizeof(vspath), k_hardcoded_vspaths[i]);
                break;
            }
        }
        
        // This is a long process, it uses all kinds of shenagians to find visual studio (damn you msvc!)
        // check previous hardcoded paths that we use to find dbghelp.dll, it is better to add to those instead 
        if (vspath[0] == 0 && rizz__win_get_vstudio_dir(vspath, sizeof(vspath))) {
            #if SX_ARCH_64BIT
                sx_strcat(vspath, sizeof(vspath), "Common7\\IDE\\Extensions\\TestPlatform\\Extensions\\Cpp\\x64");
            #elif SX_ARCH_32BIT
                sx_strcat(vspath, sizeof(vspath), "Common7\\IDE\\Extensions\\TestPlatform\\Extensions\\Cpp");
            #endif
        }

        if (vspath[0])
            sw_set_dbghelp_hintpath(vspath);

        sw_set_callstack_limits(g_mem.sw, 3, SW_MAX_FRAMES);
    #endif // SX_PLATFORM_WINDOWS

    // dummy root trace context
    g_mem.root = mem_create_trace_context("<memory>", opts, NULL);
    if (!g_mem.root) {
        sx_memory_fail();
        return false;
    }

    sx_mutex_init(&g_mem.capture.mtx);

    g_mem_imgui.collapse_items = true;

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

    sx_mutex_release(&g_mem.capture.mtx);
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

    imguix->label("Allocated size", "%$.2d", ctx->alloc_size);
    imguix->label("Peak size ", "%$.2d", ctx->peak_size);

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

static void mem_imgui_context_show_all_items(rizz_api_imgui* imgui, mem_trace_context* ctx)
{
    char text[32];
    ImGuiListClipper clipper;

    rizz__with_temp_alloc(tmp_alloc) {
        mem_item** SX_ARRAY items = NULL;
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

// void* = mem_item
static int mem_imgui_compare_callstack_hash(const void* m1, const void* m2)
{
    const mem_item_index* item1 = m1;
    const mem_item_index* item2 = m2;

    if (item1->item->callstack_hash < item2->item->callstack_hash)       return -1;
    else if (item1->item->callstack_hash > item2->item->callstack_hash)  return 1;
    else                                                                 return 0;
}

static int mem_imgui_compare_collapsed_item_id(const void* i1, const void* i2)
{
    const mem_item_collapsed* item1 = i1;
    const mem_item_collapsed* item2 = i2;

    return (g_mem_imgui.sort_spec->SortDirection == ImGuiSortDirection_Ascending) ? 
        (item1->item_idx - item2->item_idx) : 
        (item2->item_idx - item1->item_idx);
}

static int mem_imgui_compare_collapsed_item_symbol(const void* i1, const void* i2)
{
    const mem_item_collapsed* item1 = i1;
    const mem_item_collapsed* item2 = i2;
    
    int r = strcmp(item1->entry_symbol, item2->entry_symbol);

    if (g_mem_imgui.sort_spec->SortDirection == ImGuiSortDirection_Descending)
        r = -r;

    if (r == 0) {
        return (g_mem_imgui.sort_spec->SortDirection == ImGuiSortDirection_Ascending) ? 
            (item1->item_idx - item2->item_idx) : 
            (item2->item_idx - item1->item_idx); 
    } 

    return r;
}

static int mem_imgui_compare_collapsed_item_size(const void* i1, const void* i2)
{
    const mem_item_collapsed* item1 = i1;
    const mem_item_collapsed* item2 = i2;
    
    int r = 0;
    if (item1->size < item2->size)      r = -1;
    else if (item1->size > item2->size) r = 1;
    if (g_mem_imgui.sort_spec->SortDirection == ImGuiSortDirection_Descending)
        r = -r;

    if (r == 0) {
        return (g_mem_imgui.sort_spec->SortDirection == ImGuiSortDirection_Ascending) ? 
            (item1->item_idx - item2->item_idx) : 
            (item2->item_idx - item1->item_idx);         
    }

    return r;
}

static int mem_imgui_compare_collapsed_item_count(const void* i1, const void* i2)
{
    const mem_item_collapsed* item1 = i1;
    const mem_item_collapsed* item2 = i2;
    
    int r = 0;
    if (item1->count < item2->count)      r = -1;
    else if (item1->count > item2->count) r = 1;
    if (g_mem_imgui.sort_spec->SortDirection == ImGuiSortDirection_Descending)
        r = -r;    

    if (r == 0) {
        return (g_mem_imgui.sort_spec->SortDirection == ImGuiSortDirection_Ascending) ? 
            (item1->item_idx - item2->item_idx) : 
            (item2->item_idx - item1->item_idx);         
    }

    return r;
}

static mem_item_collapsed* mem_imgui_context_get_collapsed_items(mem_trace_context* ctx)
{
    mem_item_collapsed* SX_ARRAY collapsed_items = ctx->cached;
    
    // re-populate the cache if it's invalidated
    if (sx_array_count(ctx->cached) == 0 && ctx->items_list) {
        rizz__with_temp_alloc(tmp_alloc) {
            mem_item_index* SX_ARRAY items = NULL;
            {
                mem_item* item = ctx->items_list;
                int index = 0;
                while (item) {
                    mem_item_index item_indexed = {
                        .index = index,
                        .item = item
                    };
                    sx_array_push(tmp_alloc, items, item_indexed);
                    item = item->next;
                    index++;
                }
            }
            
            #if SX_PLATFORM_WINDOWS
                sw_callstack_entry callstack_entries[SW_MAX_FRAMES];
            #endif
            qsort(items, sx_array_count(items), sizeof(mem_item_index), mem_imgui_compare_callstack_hash);
            uint32_t hash = 0;
            for (int i = 0, c = sx_array_count(items); i < c; i++) {
                mem_item* item = items[i].item;

                if (item->action == MEM_ACTION_FREE)
                    continue;
                if (hash != item->callstack_hash) {
                    // create a new collapsed item
                    mem_item_collapsed citem = {
                        .item_idx = items[i].index,
                        .item = item,
                        .count = 1,
                        .size = item->size
                    };

                    if (item->num_callstack_items > 0) {    
                        #if SX_PLATFORM_WINDOWS
                            uint16_t n = sw_resolve_callstack(g_mem.sw, item->callstack, callstack_entries, 
                                                            sx_min((uint16_t)2, item->num_callstack_items));
                            sx_strcpy(citem.entry_symbol, sizeof(citem.entry_symbol), 
                                    callstack_entries[n > 1 ? 1 : 0].und_name);
                        #elif SX_PLATFORM_OSX || SX_PLATFORM_LINUX
                            Dl_info syminfo;
                            int r = dladdr(item->callstack[item->num_callstack_items > 3 ? 3 : 0], &syminfo);
                            if (syminfo.dli_sname == NULL)
                                syminfo.dli_sname = "NA";

                            char* demangled = rizz__demangle(syminfo.dli_sname);
                            if (demangled) {
                                char* paranthesis = (char*)sx_strchar(demangled, '(');
                                if (paranthesis)
                                    *paranthesis = '\0';
                            }
                            sx_strcpy(citem.entry_symbol, sizeof(citem.entry_symbol), demangled ? demangled : syminfo.dli_sname);
                            free(demangled);
                        #else
                            sx_unused(item->num_callstack_items);
                        #endif
                    } else {
                        sx_strcpy(citem.entry_symbol, sizeof(citem.entry_symbol), item->source_func);
                    }

                    sx_array_push(g_mem.alloc, ctx->cached, citem);
                    hash = item->callstack_hash;
                } else {
                    sx_assert(sx_array_count(ctx->cached) > 0);
                    mem_item_collapsed* citem = &ctx->cached[sx_array_count(ctx->cached)-1];
                    citem->item_idx = items[i].index;
                    citem->size += item->size;
                    ++citem->count;
                }
            }

            collapsed_items = ctx->cached;
        } //with_temp
    }

    int num_items = sx_array_count(collapsed_items);
    switch (g_mem_imgui.sort_spec->ColumnUserID) {
        case MEMITEM_COLLAPSED_ORDER:
            qsort(collapsed_items, num_items, sizeof(mem_item_collapsed), mem_imgui_compare_collapsed_item_id);
            break;
        case MEMITEM_COLLAPSED_SYMBOL:
            qsort(collapsed_items, num_items, sizeof(mem_item_collapsed), mem_imgui_compare_collapsed_item_symbol);
            break;
        case MEMITEM_COLLAPSED_SIZE:
            qsort(collapsed_items, num_items, sizeof(mem_item_collapsed), mem_imgui_compare_collapsed_item_size);
            break;
        case MEMITEM_COLLAPSED_COUNT:
            qsort(collapsed_items, num_items, sizeof(mem_item_collapsed), mem_imgui_compare_collapsed_item_count);
            break;
    }    

    return collapsed_items;
}

static void mem_imgui_context_show_collapsed_items(rizz_api_imgui* imgui, mem_trace_context* ctx)
{
    char text[32];
    ImGuiListClipper clipper;

    mem_trace_context_mutex_enter(ctx->options, ctx->mtx);

    imgui->TableSetupColumn("#", ImGuiTableColumnFlags_DefaultSort, 35.0f, MEMITEM_COLLAPSED_ORDER);
    imgui->TableSetupColumn("Symbol", 0, 150.0f, MEMITEM_COLLAPSED_SYMBOL);
    imgui->TableSetupColumn("Size", ImGuiTableColumnFlags_PreferSortDescending, 50.0f, MEMITEM_COLLAPSED_SIZE);
    imgui->TableSetupColumn("Count", 0, 50.0f, MEMITEM_COLLAPSED_COUNT);
    imgui->TableHeadersRow();

    ImGuiTableSortSpecs* sort_specs = imgui->TableGetSortSpecs();
    if (sort_specs) {
        if (sort_specs->SpecsCount > 0) {
            g_mem_imgui.sort_spec = &sort_specs->Specs[0];

            if (sort_specs->SpecsDirty) {
                sx_array_clear(ctx->cached);
                sort_specs->SpecsDirty = false;
            }
        }
    }

    mem_item_collapsed* collapsed_items = mem_imgui_context_get_collapsed_items(ctx);
    int num_items = sx_array_count(collapsed_items);

    if (num_items) {
        imgui->ImGuiListClipper_Begin(&clipper, num_items, -1.0f);
        while (imgui->ImGuiListClipper_Step(&clipper)) {
            for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
                imgui->TableNextRow(0, 0);
                mem_item_collapsed* item = &collapsed_items[i];

                sx_snprintf(text, sizeof(text), "%d", item->item_idx+1);
                imgui->TableNextColumn();
                imgui->PushID_Int(i);
                if (imgui->Selectable_Bool(text, g_mem_imgui.selected_item == item->item,
                                            ImGuiSelectableFlags_SpanAllColumns, SX_VEC2_ZERO)) {
                    g_mem_imgui.selected_item = item->item;
                }
                imgui->PopID();

                imgui->TableNextColumn();
                imgui->Text(item->entry_symbol);

                sx_snprintf(text, sizeof(text), "%$.2d", item->size);
                imgui->TableNextColumn();
                imgui->Text(text);

                sx_snprintf(text, sizeof(text), "%$.2d", item->count);
                imgui->TableNextColumn();
                imgui->Text(text);
            }
        }
        imgui->ImGuiListClipper_End(&clipper);
    }

    mem_trace_context_mutex_exit(ctx->options, ctx->mtx);
}

static void mem_imgui_context_items(rizz_api_imgui* imgui, rizz_api_imgui_extra* imguix, mem_trace_context* ctx)
{
    sx_unused(imguix);
    if (imgui->BeginTable("MemItems", 4, 
                        ImGuiTableFlags_Resizable|ImGuiTableFlags_BordersV|ImGuiTableFlags_BordersOuterH|
                        ImGuiTableFlags_SizingFixedFit|ImGuiTableFlags_RowBg|ImGuiTableFlags_ScrollY|ImGuiTableFlags_Sortable,
                        SX_VEC2_ZERO, 0)) {
        if (g_mem_imgui.collapse_items) {
            mem_imgui_context_show_collapsed_items(imgui, ctx);
        } else {
            mem_imgui_context_show_all_items(imgui, ctx);
        }
        imgui->EndTable();
    }
}

static void mem_imgui_open_vscode_file_loc(const char* filename, uint32_t line)
{
    char goto_str[RIZZ_MAX_PATH];
    sx_snprintf(goto_str, sizeof(goto_str), "\"%s:%u\"", filename, line);

    #if SX_PLATFORM_WINDOWS
        char vscode_path[RIZZ_MAX_PATH];
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
    imguix->label("Frame", "%lld", item->frame);
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
        #elif SX_PLATFORM_OSX || SX_PLATFORM_LINUX
            Dl_info syminfo;
            char filename[32];
            for (uint16_t i = 3; i < item->num_callstack_items; i++) {
                dladdr(item->callstack[i], &syminfo);
                if (syminfo.dli_sname == NULL)
                    syminfo.dli_sname = "NA";
                char* demangled = rizz__demangle(syminfo.dli_sname);
                if (demangled) {
                    char* paranthesis = (char*)sx_strchar(demangled, '(');
                    if (paranthesis)
                        *paranthesis = '\0';
                }

                imgui->Bullet();
                sx_os_path_basename(filename, sizeof(filename), syminfo.dli_fname);
                sx_snprintf(text, sizeof(text), "%s: %s", demangled ? demangled : syminfo.dli_sname, filename);
                if (imgui->Selectable_Bool(text, i == g_mem_imgui.selected_stack_item, ImGuiSelectableFlags_SpanAvailWidth, SX_VEC2_ZERO)) {
                    g_mem_imgui.selected_stack_item = i;
                }
                
                free(demangled);
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
        imgui->Checkbox("Collapse memory items", &g_mem_imgui.collapse_items);

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
            if (imgui->BeginChild_Str("ItemsContainer", SX_VEC2_ZERO, true, 0)) {
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

                    imgui->EndTable();
                }
            }
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

void rizz__mem_begin_capture(const char* name)
{
    sx_assertf(!g_mem.in_capture, "should end_capture before beginning a new one");

    sx_strcpy(g_mem.capture.name, sizeof(g_mem.capture.name), name);
    sx_array_clear(g_mem.capture.items);
    g_mem.capture.start_tm = sx_tm_now();
    c89atomic_flag_test_and_set_explicit(&g_mem.in_capture, c89atomic_memory_order_release);
}

SX_INLINE void mem__writef(sx_file* file, const char* fmt, ...)
{
    char str[1024];
    va_list args;
    va_start(args, fmt);
    sx_vsnprintf(str, sizeof(str), fmt, args);
    va_end(args);

    sx_file_write(file, str, sx_strlen(str));
}

static const char* mem__serialize_json_update_tabs(mem__write_json_context* jctx)
{
    if (jctx->tab[0]) {
        sx_strcpy(jctx->_tabs, sizeof(jctx->_tabs), jctx->tab);
        for (int i = 0; i < jctx->_depth; i++) {
            sx_strcat(jctx->_tabs, sizeof(jctx->_tabs), jctx->tab);
        }
    } else {
        jctx->_tabs[0] = '\0';
    }

    return jctx->_tabs;
}

static void mem__serialize_callstack_item(mem__write_json_context* jctx, 
                                          const char* file, uint32_t line, const char* func,
                                          bool last_item)
{

    mem__writef(&jctx->file, "%s{%s", jctx->_tabs, jctx->newline);
    ++jctx->_depth;
    mem__serialize_json_update_tabs(jctx);

    // convert paths to unix format, so we won't need to have escape chars
    char file_unix[RIZZ_MAX_PATH];
    sx_os_path_unixpath(file_unix, sizeof(file_unix), file);
    mem__writef(&jctx->file, "%s\"file\": \"%s\",%s", jctx->_tabs, file_unix, jctx->newline);
    mem__writef(&jctx->file, "%s\"line\": %u,%s", jctx->_tabs, line, jctx->newline);
    mem__writef(&jctx->file, "%s\"func\": \"%s\"%s", jctx->_tabs, func, jctx->newline);

    --jctx->_depth;
    mem__serialize_json_update_tabs(jctx);

    mem__writef(&jctx->file, "%s}%s%s", jctx->_tabs, !last_item ? "," : "", jctx->newline);
}

static void mem__serialize_item(mem__write_json_context* jctx, mem_item* item)
{
    const char* action = "";
    switch (item->action) {
        case MEM_ACTION_MALLOC:  action = "Malloc";  break;
        case MEM_ACTION_FREE:    action = "Free";    break;
        case MEM_ACTION_REALLOC: action = "Realloc"; break;
    }

    mem__writef(&jctx->file, "%s\"ptr\": \"0x%p\",%s", jctx->_tabs, item->ptr, jctx->newline);
    mem__writef(&jctx->file, "%s\"size\": %llu,%s", jctx->_tabs, item->size, jctx->newline);
    mem__writef(&jctx->file, "%s\"action\": \"%s\",%s", jctx->_tabs, action, jctx->newline);
    mem__writef(&jctx->file, "%s\"callstack\": [%s", jctx->_tabs, jctx->newline);
    ++jctx->_depth;
    mem__serialize_json_update_tabs(jctx);

    if (item->num_callstack_items != 0) {
        #if SX_PLATFORM_WINDOWS
            sw_callstack_entry entries[SW_MAX_FRAMES];
            uint16_t num_resolved = sw_resolve_callstack(g_mem.sw, item->callstack, entries, item->num_callstack_items);
            if (num_resolved) {
                for (int i = 0; i < num_resolved; i++) {
                    mem__serialize_callstack_item(jctx, entries[i].line_filename, entries[i].line, 
                                                  entries[i].und_name, i == num_resolved - 1);
                }
            } 
        #endif
    } else {
        mem__serialize_callstack_item(jctx, item->source_file, item->source_line, item->source_func, true);
    }

    --jctx->_depth;
    mem__serialize_json_update_tabs(jctx);
    mem__writef(&jctx->file, "%s]%s", jctx->_tabs, jctx->newline);
}

static void mem__serialize_context(mem__write_json_context* jctx, mem_trace_context* mctx)
{
    mem__writef(&jctx->file, "%s{%s", jctx->_tabs, jctx->newline);

    ++jctx->_depth;
    mem__serialize_json_update_tabs(jctx);

    mem__writef(&jctx->file, "%s\"name\": \"%s\",%s", jctx->_tabs, mctx->name, jctx->newline);
    mem__writef(&jctx->file, "%s\"num_items\": %d,%s", jctx->_tabs, mctx->num_items, jctx->newline);
    mem__writef(&jctx->file, "%s\"alloc_size\": %llu,%s", jctx->_tabs, mctx->alloc_size, jctx->newline);
    mem__writef(&jctx->file, "%s\"peak_size\": %llu,%s", jctx->_tabs, mctx->peak_size, jctx->newline);

    mem__writef(&jctx->file, "%s\"items\": [%s", jctx->_tabs, jctx->newline);
    ++jctx->_depth;
    mem__serialize_json_update_tabs(jctx);       
    bool first = true;
    for (int i = 0, ic = sx_array_count(g_mem.capture.items); i < ic; i++) {
        if (g_mem.capture.items[i]->owner == mctx) {
            mem__writef(&jctx->file, "%s%s", jctx->_tabs, !first ? "," : "");
            mem__writef(&jctx->file, "{%s", jctx->newline);
            ++jctx->_depth;
            mem__serialize_json_update_tabs(jctx);   

            mem__serialize_item(jctx, g_mem.capture.items[i]);

            --jctx->_depth;
            mem__serialize_json_update_tabs(jctx);
            mem__writef(&jctx->file, "%s}%s", jctx->_tabs, jctx->newline);
            first = false;
        }
    }
    --jctx->_depth;
    mem__serialize_json_update_tabs(jctx);
    mem__writef(&jctx->file, "%s]%s%s", jctx->_tabs, mctx->child ? "," : "", jctx->newline);

    if (mctx->child) {
        mem__writef(&jctx->file, "%s\"children\": [%s", jctx->_tabs, jctx->newline);

        ++jctx->_depth;
        mem__serialize_json_update_tabs(jctx);    

        mem_trace_context* child = mctx->child;
        while (child) {
            mem__serialize_context(jctx, child);
            child = child->next;
        }

        --jctx->_depth;
        mem__serialize_json_update_tabs(jctx);

        mem__writef(&jctx->file, "%s]%s", jctx->_tabs, jctx->newline);
    }

    --jctx->_depth;
    mem__serialize_json_update_tabs(jctx);

    mem__writef(&jctx->file, "%s}%s%s", jctx->_tabs, mctx->next ? "," : "", jctx->newline);
}

bool rizz__mem_end_capture(void)
{
    if (!g_mem.in_capture) {
        return false;
    }

    c89atomic_flag_clear(&g_mem.in_capture);

    mem__write_json_context jctx = {
        .newline = "\n",
        .tab = "  "
    };

    char filepath[RIZZ_MAX_PATH] = {0};
    #if !SX_PLATFORM_ANDROID && !SX_PLATFORM_IOS
        sx_os_path_exepath(filepath, sizeof(filepath));
    #endif
    sx_os_path_dirname(filepath, sizeof(filepath), filepath);
    sx_os_path_join(filepath, sizeof(filepath), filepath, ".memory");
    if (!sx_os_path_isdir(filepath)) {
        sx_os_mkdir(filepath);
    }
    sx_os_path_join(filepath, sizeof(filepath), filepath, g_mem.capture.name);
    sx_strcat(filepath, sizeof(filepath), ".json");

    if (!sx_file_open(&jctx.file, filepath, SX_FILE_WRITE)) {
        rizz__log_error("Could not open file '%s' for writing", filepath);
        return false;
    }

    mem__writef(&jctx.file, "{%s", jctx.newline);
    ++jctx._depth;
    mem__serialize_json_update_tabs(&jctx);

    mem__writef(&jctx.file, "%s\"name\": \"%s\",%s", jctx._tabs, g_mem.capture.name, jctx.newline);
    {
        time_t t = time(NULL);
        char time_str[64];
        sx_strcpy(time_str, sizeof(time_str), asctime(localtime(&t)));
        int len = sx_strlen(time_str);
        if (time_str[len-1] == '\n')
            time_str[len-1] = '\0';
        mem__writef(&jctx.file, "%s\"time\": \"%s\",%s", jctx._tabs, time_str, jctx.newline);
    }
    mem__writef(&jctx.file, "%s\"duration\": %f,%s", jctx._tabs, 
        sx_tm_sec(sx_tm_diff(sx_tm_now(), g_mem.capture.start_tm)), jctx.newline);
    mem__writef(&jctx.file, "%s\"total_in_capture\": %d,%s", jctx._tabs, 
        sx_array_count(g_mem.capture.items), jctx.newline);

    // serialize all contexts, but just populate them with the items in the capture
    mem__writef(&jctx.file, "%s\"items\": [%s", jctx._tabs, jctx.newline);

    ++jctx._depth;
    mem__serialize_json_update_tabs(&jctx);

    sx_assert(g_mem.root);
    mem_trace_context* mctx = g_mem.root->child;
    while (mctx) {
        mem__serialize_context(&jctx, mctx);
        mctx = mctx->next;
    }

    --jctx._depth;
    mem__serialize_json_update_tabs(&jctx);
    mem__writef(&jctx.file, "%s]%s", jctx._tabs, jctx.newline);

    mem__writef(&jctx.file, "}%s", jctx.newline);
    
    sx_file_close(&jctx.file);

    // remove all "freed" items
    for (int i = 0, ic = sx_array_count(g_mem.capture.items); i < ic; i++) {
        mem_item* item = g_mem.capture.items[i];
        if (item->freed) {
            mem_destroy_trace_item(item->owner, item);
        }
    }

    return true;
}

