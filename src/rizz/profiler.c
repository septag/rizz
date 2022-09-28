//
// Copyright 2021 Sepehr Taghdisian (septag@github). All rights reserved.
// License: https://github.com/septag/rizz#license-bsd-2-clause
//

#include "internal.h"

#include "rizz/rizz.h"

#include "sx/allocator.h"
#include "sx/array.h"
#include "sx/io.h"
#include "sx/handle.h"
#include "sx/macros.h"
#include "sx/threads.h"
#include "sx/string.h"
#include "sx/os.h"
#include "sx/hash.h"

typedef struct profile_item {
    char     name[32];
    uint64_t start_tm;
    uint64_t duration;
    uint32_t thread_id;
    uint32_t caller_line;
    char     caller_file[32];
} profile_item;

typedef struct profile_item_track {
    uint32_t name_hash;
    int      index;
} profile_item_track;

typedef struct profile_capture_context {
    char filename[32];
    sx_mutex mtx;
    profile_item* SX_ARRAY items;
    profile_item_track* SX_ARRAY item_stack;   // keep trace of profiler stack, so we don't do any mistakes in begin/end calls
    uint32_t last_name_hash;
} profile_capture_context;

typedef struct profile_state
{
    const sx_alloc* alloc;
    sx_mutex capture_context_mtx;
    sx_handle_pool* capture_context_handles;               // profile_capture_context
    profile_capture_context* SX_ARRAY capture_contexts;    // capture-profiler is mainly used for load times and one-time captures
} profile_state;

static profile_state g_profile;

bool rizz__profile_init(const sx_alloc* alloc)
{
    #if RIZZ_CONFIG_PROFILER
        g_profile.alloc = alloc;
    
        sx_mutex_init(&g_profile.capture_context_mtx);
        g_profile.capture_context_handles = sx_handle_create_pool(g_profile.alloc, 16);
        if (!g_profile.capture_context_handles)
            return false;
    #else
        sx_unused(alloc);
    #endif
    return true;
}

void rizz__profile_release(void)
{
    #if RIZZ_CONFIG_PROFILER
        // go through all the open handles and end them
        for (int i = 0; i < g_profile.capture_context_handles->count; i++) {
            sx_handle_t h = sx_handle_at(g_profile.capture_context_handles, i);
            the__core.profile_capture_end((rizz_profile_capture) { .id = h });
        }
    
        sx_mutex_release(&g_profile.capture_context_mtx);
        sx_handle_destroy_pool(g_profile.capture_context_handles, g_profile.alloc);
    #endif
}

rizz_profile_capture rizz__profile_capture_create(const char* filename)
{
    #if RIZZ_CONFIG_PROFILER
        rizz_profile_capture handle = { 0 };
        sx_mutex_lock(g_profile.capture_context_mtx) {
            handle.id = sx_handle_new_and_grow(g_profile.capture_context_handles, g_profile.alloc);
            sx_assert(handle.id);
            
            profile_capture_context profiler = {0};
            sx_mutex_init(&profiler.mtx);
            sx_strcpy(profiler.filename, sizeof(profiler.filename), filename);
            
            int index = sx_handle_index(handle.id);
            if (index >= sx_array_count(g_profile.capture_contexts)) {
                sx_array_push(g_profile.alloc, g_profile.capture_contexts, profiler);
            } else {
                g_profile.capture_contexts[index] = profiler;
            }
        }
        return handle;
    #else
        sx_unused(filename);
        return (rizz_profile_capture) { 0 };
    #endif
}

void rizz__profile_capture_end(rizz_profile_capture cid)
{
    #if RIZZ_CONFIG_PROFILER
        if (cid.id == 0) 
            return;

        sx_mutex_lock(g_profile.capture_context_mtx) {
            // write all entries to the file and delete the profiler
            sx_assert_always(sx_handle_valid(g_profile.capture_context_handles, cid.id));
            
            profile_capture_context* profiler = &g_profile.capture_contexts[sx_handle_index(cid.id)];
            sx_handle_del(g_profile.capture_context_handles, cid.id);
            
            char trace_filepath[RIZZ_MAX_PATH];
            #if !SX_PLATFORM_ANDROID && !SX_PLATFORM_IOS
                sx_os_path_exepath(trace_filepath, sizeof(trace_filepath));
            #endif
            sx_os_path_dirname(trace_filepath, sizeof(trace_filepath), trace_filepath);
            sx_os_path_join(trace_filepath, sizeof(trace_filepath), trace_filepath, ".profiler");
            
            if (!sx_os_path_isdir(trace_filepath)) {
                sx_os_mkdir(trace_filepath);
            }
            
            sx_os_path_join(trace_filepath, sizeof(trace_filepath), trace_filepath, profiler->filename);
            sx_strcat(trace_filepath, sizeof(trace_filepath), ".json");
            
            sx_file f;
            if (!sx_file_open(&f, trace_filepath, SX_FILE_WRITE)) {
                rizz__log_error("[profiler] could not open '%s' for writing", trace_filepath);
                sx_mutex_exit(&g_profile.capture_context_mtx);
                return;
            }
            
            char entry[1024];
            uint32_t pid = sx_os_getpid();
            
            sx_file_write_text(&f, "[\n");
            
            for (int i = 0, c = sx_array_count(profiler->items); i < c; i++) {
                profile_item* item = &profiler->items[i];
                sx_snprintf(entry, sizeof(entry), 
                    "\t{\"ph\": \"X\", \"pid\": %u, \"tid\": %u, \"ts\": %llu, \"dur\": %llu, \"name\": \"%s\", \"args\": {\"caller\":\"%s@%u\"}}",
                    pid, item->thread_id, item->start_tm/1000, item->duration/1000, item->name, item->caller_file, item->caller_line);
                if (i != c - 1) 
                    sx_strcat(entry, sizeof(entry), ",\n");
                else
                    sx_strcat(entry, sizeof(entry), "\n");
                sx_file_write_text(&f, entry);
            }
            
            sx_file_write_text(&f, "\n]\n");
            sx_file_close(&f);
            
            // cleanup
            sx_array_free(g_profile.alloc, profiler->items);
            sx_mutex_release(&profiler->mtx);

            rizz__log_info("(profiler) chrome trace file saved to: %s", trace_filepath);
        } // lock
    #else
        sx_unused(cid);
    #endif
}

void rizz__profile_capture_sample_begin(rizz_profile_capture cid, const char* name, const char* file, uint32_t line)
{
    #if RIZZ_CONFIG_PROFILER
        if (cid.id == 0) 
            return;

        sx_mutex_enter(&g_profile.capture_context_mtx);
        profile_capture_context* profiler = &g_profile.capture_contexts[sx_handle_index(cid.id)];
        sx_mutex_exit(&g_profile.capture_context_mtx);
        
        sx_mutex_lock(profiler->mtx) {
            profile_item item = {
                .start_tm = sx_tm_now(),
                .thread_id = sx_thread_tid(),
                .caller_line = line
            };
            sx_strcpy(item.name, sizeof(item.name), name);
            if (file)
                sx_os_path_basename(item.caller_file, sizeof(item.caller_file), file);
            sx_array_push(g_profile.alloc, profiler->items, item);
    
            profile_item_track track = {
                .name_hash = sx_hash_fnv32_str(item.name),
                .index = sx_array_count(profiler->items) - 1,
    
            };
            sx_array_push(g_profile.alloc, profiler->item_stack, track);
    
            profiler->last_name_hash = track.name_hash;
        } // lock
    #else
        sx_unused(cid);
        sx_unused(name);
        sx_unused(file);
        sx_unused(line);
    #endif
}

void rizz__profile_capture_sample_end(rizz_profile_capture cid)
{
    #if RIZZ_CONFIG_PROFILER
        if (cid.id == 0) 
            return;
        sx_mutex_enter(&g_profile.capture_context_mtx);
        profile_capture_context* profiler = &g_profile.capture_contexts[sx_handle_index(cid.id)];
        sx_mutex_exit(&g_profile.capture_context_mtx);
        
        sx_mutex_lock(profiler->mtx) {
            sx_assert(sx_array_count(profiler->item_stack) > 0);
            profile_item_track track = sx_array_last(profiler->item_stack);
            sx_array_pop_last(profiler->item_stack);
    
            profile_item* item = &profiler->items[track.index];
            sx_assertf(track.name_hash == profiler->last_name_hash, "invalid begin/end order for trace profile '%s'", item->name);
            item->duration = sx_tm_diff(sx_tm_now(), item->start_tm);
    
            int item_stack_count = sx_array_count(profiler->item_stack);
            profiler->last_name_hash = item_stack_count ? profiler->item_stack[item_stack_count-1].name_hash : 0;
        } // lock
    #else
        sx_unused(cid);
    #endif
}

