#include "rizz/imgui.h"
#include "rizz/rizz.h"

#include "sx/allocator.h"
#include "sx/array.h"
#include "sx/os.h"
#include "sx/ringbuffer.h"
#include "sx/string.h"
#include "sx/linear-buffer.h"

#include <float.h>

extern rizz_api_imgui the__imgui;
RIZZ_STATE rizz_api_core* the_core;

typedef struct imgui__log_entry_ref {
    int offset;
    int size;
} imgui__log_entry_ref;

typedef struct imgui__log_context {
    const sx_alloc* alloc;
    sx_ringbuffer* buffer;
} imgui__log_context;

static imgui__log_context g_log;

#define LOG_ENTRY sx_makefourcc('_', 'L', 'O', 'G')

void imgui__show_log(bool* p_open)
{
    static const sx_vec4 k_log_colors[_RIZZ_LOG_LEVEL_COUNT] = {
        {{1.0f, 0, 0, 1.0f}},
        {{1.0f, 1.0f, 0, 1.0f}},
        {{0.9f, 0.9f, 0.9f, 1.0f}},
        {{0.65f, 0.65f, 0.65f, 1.0f}},
        {{0.65f, 0.65f, 0.65f, 1.0f}}
    };

    the__imgui.SetNextWindowSizeConstraints(sx_vec2f(600.0f, 100.0f), sx_vec2f(FLT_MAX, FLT_MAX),
                                            NULL, NULL);
    if (the__imgui.Begin("Log", p_open, 0)) {
        ImGuiListClipper clipper;

        if (g_log.buffer->size) {
            the__imgui.Columns(3, NULL, false);
            the__imgui.SetColumnWidth(0, 500.0f);
            the__imgui.Text("Text");
            the__imgui.NextColumn();
            the__imgui.SetColumnWidth(1, 100.0f);
            the__imgui.Text("File");
            the__imgui.NextColumn();
            the__imgui.SetColumnWidth(2, 35.0f);
            the__imgui.Text("Line");
            the__imgui.NextColumn();
            the__imgui.Separator();

            the__imgui.Columns(1, NULL, false);
            sx_vec2 region;
            the__imgui.GetContentRegionAvail(&region);
            the__imgui.BeginChildStr("LogEntries", sx_vec2f(region.x, -1.0f), false, 0);
            the__imgui.Columns(3, NULL, false);

            // Read all log addresses by jumping through the buffer
            int offset = g_log.buffer->start;
            int size_read = 0;
            const uint32_t log_tag = LOG_ENTRY;
            imgui__log_entry_ref* entries = NULL;    // entries are offsets to the ring-buffer
            rizz_temp_alloc_begin(tmp_alloc);
            sx_array_reserve(tmp_alloc, entries, 16);
            do {
                uint32_t tag;
                int last_offset = offset;
                int read =
                    sx_ringbuffer_read_noadvance(g_log.buffer, &tag, sizeof(log_tag), &offset);
                if (tag == log_tag) {
                    int entry_sz;
                    read += sx_ringbuffer_read_noadvance(g_log.buffer, &entry_sz, sizeof(entry_sz),
                                                         &offset);
                    imgui__log_entry_ref ref =
                        (imgui__log_entry_ref){ .offset = offset, .size = (int)entry_sz };
                    sx_array_push(tmp_alloc, entries, ref);
                    offset = (offset + (int)entry_sz) % g_log.buffer->capacity;
                    read += (int)entry_sz;
                } else {
                    // advance only 1-byte and check for the existing tag
                    offset = (last_offset + 1) % g_log.buffer->capacity;
                    read = 1;
                }
                size_read += read;
            } while(size_read < g_log.buffer->size);
            
            int num_items = sx_array_count(entries);

            the__imgui.ImGuiListClipper_Begin(&clipper, num_items, -1.0f);
            while (the__imgui.ImGuiListClipper_Step(&clipper)) {
                int start = num_items - clipper.DisplayStart - 1;
                int end = num_items - clipper.DisplayEnd;
                for (int i = start; i >= end; i--) {
                    const imgui__log_entry_ref* ref = &entries[i];
                    offset = ref->offset;
                    rizz_log_entry* entry = sx_malloc(tmp_alloc, ref->size);
                    sx_assert(entry);
                    sx_ringbuffer_read_noadvance(g_log.buffer, entry, ref->size, &offset);

                    const char* text = (const char*)entry + (uintptr_t)entry->text;
                    const char* source_file = (const char*)entry + (uintptr_t)entry->source_file;

                    sx_assert(entry->type >= 0 && entry->type < _RIZZ_LOG_LEVEL_COUNT);
                    the__imgui.SetColumnWidth(0, 500.0f);
                    the__imgui.TextColored(k_log_colors[entry->type], text);
                    the__imgui.NextColumn();

                    the__imgui.SetColumnWidth(1, 100.0f);
                    the__imgui.TextEx(source_file, source_file + entry->source_file_len, 0);
                    the__imgui.NextColumn();

                    the__imgui.SetColumnWidth(2, 35.0f);
                    the__imgui.Text("%d", entry->line);
                    the__imgui.NextColumn();

                    sx_free(tmp_alloc, entry);
                }
            }
            the__imgui.ImGuiListClipper_End(&clipper);
            the__imgui.EndChild();

            sx_array_free(tmp_alloc, entries);
            rizz_temp_alloc_end(tmp_alloc);
        }    //
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

    rizz_temp_alloc_begin(tmp_alloc);
    rizz_log_entry* _entry = (rizz_log_entry*)sx_linear_buffer_alloc(&item_buff, tmp_alloc);
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
    rizz_temp_alloc_end(tmp_alloc);
}

bool imgui__log_init(rizz_api_core* core, const sx_alloc* alloc, uint32_t buffer_size)
{
    the_core = core;

    g_log.alloc = alloc;
    
    g_log.buffer = sx_ringbuffer_create(alloc, buffer_size);
    if (!g_log.buffer) {
        sx_out_of_memory();
        return false;
    }

    the_core->register_log_backend("imgui_log", imgui__log_entryfn, NULL);
    
    return true;
}

void imgui__log_release(void)
{
    the_core->unregister_log_backend("imgui_log");

    if (g_log.buffer) {
        sx_ringbuffer_destroy(g_log.buffer, g_log.alloc);
    }
}