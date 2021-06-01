#include "rizz/imgui.h"
#include "rizz/rizz.h"

#include "sx/allocator.h"
#include "sx/array.h"
#include "sx/os.h"
#include "sx/ringbuffer.h"
#include "sx/string.h"
#include "sx/linear-buffer.h"
#include "sx/math-vec.h"

#include "imgui-internal.h"

#include <float.h>
#include <alloca.h>

extern rizz_api_imgui the__imgui;
RIZZ_STATE rizz_api_core* the_core;
RIZZ_STATE rizz_api_app* the_app;

typedef struct imgui__log_entry_ref {
    int offset;
    int size;
} imgui__log_entry_ref;

typedef enum imgui__filter_types {
    LOG_FILTER_ERROR = 0x1, 
    LOG_FILTER_WARNING = 0x2, 
    LOG_FILTER_INFO = 0x4, 
    LOG_FILTER_VERBOSE = 0x8,
    LOG_FILTER_DEBUG = 0x10,
} imgui__filter_types;

typedef struct imgui__log_context {
    const sx_alloc* alloc;
    sx_ringbuffer* buffer;
    uint32_t filter_channels;
    uint32_t filter_types;
    imgui__log_entry_ref* cached;   // sx_array
    int selected;
    bool reset_focus;
    bool toggle;
    bool* popen;
    bool focus_command;
    bool show_filters;
} imgui__log_context;

static imgui__log_context g_log;

#define LOG_ENTRY sx_makefourcc('_', 'L', 'O', 'G')

static bool imgui__bitselector(const char* label, uint32_t* bits)
{
    float width = the__imgui.GetWindowContentRegionWidth();
    int items_in_row = 32;
    float bit_btn_w = width / (float)items_in_row;
    int num_rows = 1;
    while (bit_btn_w < 22.0f && items_in_row) {
        num_rows ++;
        items_in_row <<= 1;
        bit_btn_w = width / (float)items_in_row;
    }
    if (items_in_row == 0) {
        return false;
    }

    bool pressed = false;
    
    the__imgui.SameLine(0, -1.0f);
    if (the__imgui.SmallButton("All")) {
        *bits = 0xffffffff;
        pressed = true;
    }
    the__imgui.SameLine(0, -1.0f);
    if (the__imgui.SmallButton("None")) {
        *bits = 0;
        pressed = true;
    }
    the__imgui.SameLine(0, -1.0f);
    the__imgui.LabelText(label, "0x%x (%u)", *bits, *bits);

    the__imgui.Columns(items_in_row, NULL, false);
    for (int i = 0; i < num_rows; i++) {
        for (int c = 0; c < items_in_row; c++) {
            the__imgui.SetColumnWidth(c, 22.0f);
            int bit_index = c + i*items_in_row;
            the__imgui.PushOverrideID(bit_index);

            bool on = (*bits  & (0x1 << bit_index)) ? true : false;
            the__imgui.PushStyleColor_U32(ImGuiCol_Button, on ? 0xff00ff00 : 0xff000000);
            if (the__imgui.SmallButton("  ")) {
                on = !on;
                *bits = on ? (*bits | (0x1 << bit_index)) : (*bits & ~(0x1 << bit_index));
                pressed = true;
            }
            the__imgui.PopStyleColor(1);
            the__imgui.PopID();

            the__imgui.NextColumn();
        }

        for (int c = 0; c < items_in_row; c++) {
            if (c % 4 == 0) {
                the__imgui.TextColored(sx_vec4f(0.3f, 0.3f, 0.3f, 1.0f), "%d", c);
            }
            the__imgui.NextColumn();
        }
    }

    return pressed;
}

static void imgui__show_command_console(void)
{
    char cmd[512] = {0};

    the__imgui.PushItemWidth(-50);
    the__imgui.SetItemDefaultFocus();
    if (g_log.focus_command) {
        the__imgui.SetKeyboardFocusHere(0);
        g_log.focus_command = false;
    }
    if (the__imgui.InputTextWithHint("##commands", "Enter commands", cmd, sizeof(cmd), 
                                     ImGuiInputTextFlags_EnterReturnsTrue, NULL, NULL)) {
        the_core->execute_console_command(cmd);
        g_log.reset_focus = true;
    }
    the__imgui.PopItemWidth();

    the__imgui.SameLine(0, 8);
    if (the__imgui.Button("Filters", sx_vec2f(-1, the__imgui.GetFrameHeight())))
        g_log.show_filters = !g_log.show_filters;

    if (g_log.reset_focus) {
        the__imgui.SetKeyboardFocusHere(-1);
        g_log.reset_focus = false;
    }
}

static bool imgui__entry_filtered_out(rizz_log_level type, uint32_t channels)
{
    uint32_t filter_channels = g_log.filter_channels;
    uint32_t filter_types = g_log.filter_types;

    switch (type) {
    case RIZZ_LOG_LEVEL_INFO:      if (!(filter_types & LOG_FILTER_INFO))    return true;   break;
    case RIZZ_LOG_LEVEL_DEBUG:     if (!(filter_types & LOG_FILTER_DEBUG))   return true;   break; 
    case RIZZ_LOG_LEVEL_VERBOSE:   if (!(filter_types & LOG_FILTER_VERBOSE)) return true;   break;
    case RIZZ_LOG_LEVEL_WARNING:   if (!(filter_types & LOG_FILTER_WARNING)) return true;   break;
    case RIZZ_LOG_LEVEL_ERROR:     if (!(filter_types & LOG_FILTER_ERROR))   return true;   break;
    default:    break;
    }

    return !(filter_channels & channels) ? true : false;
}

static void imgui__update_entry_cache(void)
{
    const sx_alloc* alloc = g_log.alloc;
    int offset = g_log.buffer->start;
    int size_read = 0;
    const uint32_t log_tag = LOG_ENTRY;

    do {
        uint32_t tag;
        int last_offset = offset;
        int read = sx_ringbuffer_read_noadvance(g_log.buffer, &tag, sizeof(log_tag), &offset);
        if (tag == log_tag) {
            int entry_sz;
            read += sx_ringbuffer_read_noadvance(g_log.buffer, &entry_sz, sizeof(entry_sz), &offset);

            // read channels/type for filtering
            rizz_log_level type;
            uint32_t channels;
            int cur_offset = offset;
            sx_ringbuffer_read_noadvance(g_log.buffer, &type, sizeof(type), &offset);
            sx_ringbuffer_read_noadvance(g_log.buffer, &channels, sizeof(channels), &offset);
            offset = cur_offset;

            if (!imgui__entry_filtered_out(type, channels)) {
                imgui__log_entry_ref ref = (imgui__log_entry_ref){ .offset = offset, .size = (int)entry_sz };
                sx_array_push(alloc, g_log.cached, ref);
            }
            offset = (offset + (int)entry_sz) % g_log.buffer->capacity;
            read += (int)entry_sz;
        } else {
            // advance only 1-byte and check for the existing tag
            offset = (last_offset + 1) % g_log.buffer->capacity;
            read = 1;
        }
        size_read += read;
    } while (size_read < g_log.buffer->size);

}

void imgui__show_log(bool* p_open)
{
    g_log.toggle = true;
    g_log.popen = p_open;
}

static void imgui__draw_log(bool* p_open)
{
    static const sx_vec4 k_log_colors[_RIZZ_LOG_LEVEL_COUNT] = {
        {{1.0f, 0, 0, 1.0f}},
        {{1.0f, 1.0f, 0, 1.0f}},
        {{0.9f, 0.9f, 0.9f, 1.0f}},
        {{0.65f, 0.65f, 0.65f, 1.0f}},
        {{0.65f, 0.65f, 0.65f, 1.0f}}
    };

    the__imgui.SetNextWindowSizeConstraints(sx_vec2f(720.0f, 200.0f), sx_vec2f(FLT_MAX, FLT_MAX), NULL, NULL);

    if (the__imgui.Begin("Log", p_open, 0)) {
        imgui__show_command_console();

        if (g_log.show_filters) {
            bool filter_changed = false;
            filter_changed |= the__imgui.CheckboxFlags_UintPtr("Error", &g_log.filter_types, LOG_FILTER_ERROR);   the__imgui.SameLine(0, -1);
            filter_changed |= the__imgui.CheckboxFlags_UintPtr("Warning", &g_log.filter_types, LOG_FILTER_WARNING); the__imgui.SameLine(0, -1);
            filter_changed |= the__imgui.CheckboxFlags_UintPtr("Info", &g_log.filter_types, LOG_FILTER_INFO); the__imgui.SameLine(0, -1);
            filter_changed |= the__imgui.CheckboxFlags_UintPtr("Verbose", &g_log.filter_types, LOG_FILTER_VERBOSE); the__imgui.SameLine(0, -1);
            filter_changed |= the__imgui.CheckboxFlags_UintPtr("Debug", &g_log.filter_types, LOG_FILTER_DEBUG); the__imgui.SameLine(0, -1);
            filter_changed |= imgui__bitselector("##Channels", &g_log.filter_channels);

            if (filter_changed) {
                sx_array_clear(g_log.cached);
            }
        }

        ImGuiListClipper clipper;
        float width = the__imgui.GetWindowContentRegionWidth();
        float text_width = width - 170.0f;

        if (g_log.buffer->size && 
            the__imgui.BeginTable("Sound Sources", 3, 
                                  ImGuiTableFlags_Resizable|ImGuiTableFlags_RowBg|ImGuiTableFlags_ScrollY|
                                  ImGuiTableFlags_BordersOuterH|ImGuiTableFlags_SizingStretchProp, 
                                  SX_VEC2_ZERO, 0)) {

            the__imgui.TableSetupColumn("Text", 0, text_width, 0);
            the__imgui.TableSetupColumn("File", 0, 110.0f, 0);
            the__imgui.TableSetupColumn("Line", 0, 60.0f, 0);
            the__imgui.TableHeadersRow();

            if (sx_array_count(g_log.cached) == 0) {
                imgui__update_entry_cache();
            }
            imgui__log_entry_ref* entries = g_log.cached;    // entries are offsets to the ring-buffer
            int num_items = sx_array_count(entries);

            rizz_with_temp_alloc(tmp_alloc) {
                the__imgui.ImGuiListClipper_Begin(&clipper, num_items, -1.0f);
                while (the__imgui.ImGuiListClipper_Step(&clipper)) {
                    for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
                        the__imgui.TableNextRow(0, 0);

                        const imgui__log_entry_ref* ref = &entries[i];
                        int offset = ref->offset;
                        rizz_log_entry* entry = sx_malloc(tmp_alloc, ref->size);
                        sx_assert(entry);
                        sx_ringbuffer_read_noadvance(g_log.buffer, entry, ref->size, &offset);

                        const char* text = (const char*)entry + (uintptr_t)entry->text;
                        const char* source_file = (const char*)entry + (uintptr_t)entry->source_file;

                        sx_assert(entry->type >= 0 && entry->type < _RIZZ_LOG_LEVEL_COUNT);
                        the__imgui.TableNextColumn();
                        the__imgui.PushStyleColor_Vec4(ImGuiCol_Text, k_log_colors[entry->type]);
                        if (the__imgui.Selectable_Bool(text, g_log.selected == i, 
                            ImGuiSelectableFlags_SpanAllColumns|ImGuiSelectableFlags_AllowDoubleClick, sx_vec2f(0, 0))) {
                            // copy to clipboard
                            int len = entry->text_len + entry->source_file_len + 32;
                            char* clipboard_text = alloca(len);
                            sx_assert_always(clipboard_text);
                            sx_snprintf(clipboard_text, len, "(%s:%d) %s", source_file, entry->line, text);
                            the_app->set_clipboard_string(clipboard_text);
                            g_log.selected = i;
                        }
                        the__imgui.PopStyleColor(1);

                        the__imgui.TableNextColumn();
                        the__imgui.TextEx(source_file, source_file + entry->source_file_len, 0);

                        the__imgui.TableNextColumn();
                        the__imgui.Text("%d", entry->line);
                    }
                }
            }   // with temp_alloc


            if (the__imgui.GetScrollY() >= the__imgui.GetScrollMaxY()) {
                the__imgui.SetScrollHereY(1.0f);
            }

            the__imgui.ImGuiListClipper_End(&clipper);
            the__imgui.EndTable();
        }   

    }
    the__imgui.End();
}


static void imgui__log_entryfn(const rizz_log_entry* entry, void* user)
{
    sx_unused(user);
    
    char filename[64];
    int source_file_len = 0;
    if (entry->source_file && entry->source_file_len > 0) {
        sx_os_path_basename(filename, sizeof(filename), entry->source_file);
        source_file_len = sx_strlen(filename);
    } else {
        filename[0] = '\0';
    }

    sx_linear_buffer item_buff;
    sx_linear_buffer_init(&item_buff, rizz_log_entry, 0);
    sx_linear_buffer_addtype(&item_buff, rizz_log_entry, char, text, entry->text_len + 1, 0);
    sx_linear_buffer_addtype(&item_buff, rizz_log_entry, char, source_file, source_file_len + 1, 0);

    const sx_alloc* tmp_alloc = the_core->tmp_alloc_push();
    sx_scope(the_core->tmp_alloc_pop()) {
        rizz_log_entry* _entry = (rizz_log_entry*)sx_linear_buffer_calloc(&item_buff, tmp_alloc);
        _entry->type = entry->type;
        _entry->channels = entry->channels;
        _entry->text_len = entry->text_len;
        _entry->source_file_len = source_file_len;
        sx_strcpy((char*)_entry->text, entry->text_len + 1, entry->text);
        if (source_file_len > 0) {
            sx_strcpy((char*)_entry->source_file, source_file_len + 1, filename);
        } else {
            ((char*)_entry->source_file)[0] = '\0';
        }
        _entry->line = entry->line;

        // write to ring-buffer
        // first check if expected size is met, if not, just advance some bytes in ring-buffer to make 
        // this data fit into it
        int expect_size = sx_ringbuffer_expect_write(g_log.buffer);
        int needed_size = (int)(sizeof(uint32_t) + sizeof(item_buff.size) + item_buff.size);
        if (expect_size < needed_size) {
            sx_ringbuffer_read(g_log.buffer, NULL, needed_size - expect_size);
            sx_assert(sx_ringbuffer_expect_write(g_log.buffer) == needed_size);
        }

        const uint32_t log_tag = LOG_ENTRY;
        const int entry_sz = (int)item_buff.size;
        sx_ringbuffer_write(g_log.buffer, &log_tag, sizeof(log_tag));
        sx_ringbuffer_write(g_log.buffer, &entry_sz, sizeof(entry_sz));

        // HACK: instead of actual pointers, change them to offsets that start from entry pointer
        _entry->text = (const char*)((uintptr_t)_entry->text - (uintptr_t)_entry);
        _entry->source_file = (const char*)((uintptr_t)_entry->source_file - (uintptr_t)_entry);
        sx_ringbuffer_write(g_log.buffer, _entry, entry_sz);

        sx_free(tmp_alloc, _entry);
    }  // scope

    sx_array_clear(g_log.cached);
}

void imgui__log_update(void)
{
    if (g_log.toggle) {
        imgui__draw_log(g_log.popen);
    }
}

static int imgui__toggle_log_cb(int argc, char* argv[], void* user)
{
    sx_unused(argc);
    sx_unused(argv);
    sx_unused(user);

    g_log.toggle = !g_log.toggle;
    g_log.popen = NULL;
    g_log.focus_command = true;
    return 0;
}

bool imgui__log_init(rizz_api_core* core, rizz_api_app* app, const sx_alloc* alloc, uint32_t buffer_size)
{
    the_core = core;
    the_app = app;

    g_log.alloc = alloc;
    
    g_log.buffer = sx_ringbuffer_create(alloc, buffer_size);
    if (!g_log.buffer) {
        sx_out_of_memory();
        return false;
    }

    the_core->register_log_backend("imgui_log", imgui__log_entryfn, NULL);
    g_log.filter_channels = 0xffffffff;
    g_log.selected = -1;
    g_log.filter_types = 0xffffffff;

    the_core->register_console_command("toggle_log", imgui__toggle_log_cb, "`", NULL);
    
    return true;
}

void imgui__log_release(void)
{
    sx_array_free(g_log.alloc, g_log.cached);
    the_core->unregister_log_backend("imgui_log");

    if (g_log.buffer) {
        sx_ringbuffer_destroy(g_log.buffer, g_log.alloc);
    }
}