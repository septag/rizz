//
// Copyright 2019 Sepehr Taghdisian (septag@github). All rights reserved.
// License: https://github.com/septag/rizz#license-bsd-2-clause
//
#include "config.h"

#include "rizz/asset.h"
#include "rizz/core.h"
#include "rizz/entry.h"
#include "rizz/graphics.h"
#include "rizz/http.h"
#include "rizz/imgui.h"
#include "rizz/plugin.h"
#include "rizz/reflect.h"
#include "rizz/vfs.h"

#include "sx/allocator.h"
#include "sx/array.h"
#include "sx/atomic.h"
#include "sx/hash.h"
#include "sx/jobs.h"
#include "sx/os.h"
#include "sx/rng.h"
#include "sx/stack-alloc.h"
#include "sx/string.h"
#include "sx/threads.h"
#include "sx/timer.h"
#include "sx/virtual-alloc.h"

#include <alloca.h>
#include <stdio.h>
#include <time.h>

#include "Remotery.h"

#if SX_PLATFORM_ANDROID
#    include <android/log.h>
#endif

#if SX_COMPILER_MSVC
__declspec(dllimport) void __stdcall OutputDebugStringA(const char* lpOutputString);
#endif

#define rmt__begin_cpu_sample(name, flags, hash) \
    RMT_OPTIONAL(RMT_ENABLED, _rmt_BeginCPUSample(name, flags, hash_cache))
#define rmt__end_cpu_sample() RMT_OPTIONAL(RMT_ENABLED, _rmt_EndCPUSample())

////////////////////////////////////////////////////////////////////////////////////////////////////
// sjson implementation, external modules (non-static build) should implement it for themselves
// clang-format off
#define sjson_malloc(user, size)        sx_malloc((const sx_alloc*)(user), (size))
#define sjson_free(user, ptr)           sx_free((const sx_alloc*)(user), (ptr))
#define sjson_realloc(user, ptr, size)  sx_realloc((const sx_alloc*)(user), (ptr), (size))
#define sjson_assert(e)                 sx_assert(e)
#define sjson_out_of_memory()           sx_out_of_memory()
#define sjson_snprintf                  sx_snprintf
#define sjson_strlen(str)               sx_strlen((str))
#define sjson_strcpy(a, s, b)           sx_strcpy((a), (s), (b))
#define SJSON_IMPLEMENT
#include "sjson/sjson.h"

#define DEFAULT_TMP_SIZE    0x500000    // 5mb

#if SX_PLATFORM_WINDOWS || SX_PLATFORM_IOS || SX_PLATFORM_ANDROID
#   define TERM_COLOR_RESET     ""
#   define TERM_COLOR_RED       ""
#   define TERM_COLOR_YELLOW    ""
#   define TERM_COLOR_GREEN     ""
#   define TERM_COLOR_DIM       ""
#else
#   define TERM_COLOR_RESET     "\033[0m"
#   define TERM_COLOR_RED       "\033[31m"
#   define TERM_COLOR_YELLOW    "\033[33m"
#   define TERM_COLOR_GREEN     "\033[32m"
#   define TERM_COLOR_DIM       "\033[2m"
#endif
// clang-format on

static const char* k__memid_names[_RIZZ_MEMID_COUNT] = { "Core",  "Graphics",   "Audio",
                                                         "IO",    "Reflection", "Other",
                                                         "Debug", "Toolset",    "Game" };


typedef struct rizz__core_tmpalloc {
    sx_stackalloc stack_alloc;
    int* offset_stack;    // sx_array - keep offsets in a stack for push()/pop()
} rizz__core_tmpalloc;

typedef struct rizz__core_cmd {
    char name[32];
    rizz_core_cmd_cb* callback;
} rizz__core_cmd;

typedef struct rizz__proxy_alloc_header {
    intptr_t size;
    uint32_t ptr_offset;
    int track_item_idx;
} rizz__proxy_alloc_header;

typedef struct rizz__track_alloc {
    sx_alloc alloc;
    int mem_id;
    const char* name;
    int64_t size;
    int64_t peak;
    sx_lock_t _lk;
    rizz_track_alloc_item* items;    // sx_array
} rizz__track_alloc;

typedef struct rizz__tls_var {
    uint32_t name_hash;
    void* user;
    sx_tls tls;
    void* (*init_cb)(int thread_idx, uint32_t thread_id, void* user);
} rizz__tls_var;

typedef struct rizz__core {
    const sx_alloc* heap_alloc;
    sx_alloc heap_proxy_alloc;
    rizz__track_alloc track_allocs[_RIZZ_MEMID_COUNT];
    sx_atomic_int heap_count;
    sx_atomic_size heap_size;
    sx_atomic_size heap_max;

    sx_rng rng;
    sx_job_context* jobs;
    sx_coro_context* coro;

    uint32_t flags;    // sx_core_flags

    int64_t frame_idx;
    uint64_t elapsed_tick;
    uint64_t delta_tick;
    uint64_t last_tick;
    float fps_mean;
    float fps_frame;

    char app_name[32];
    char logfile[32];
    uint32_t app_ver;

    rizz__core_tmpalloc* tmp_allocs;         // count: num_workers
    rizz__gfx_cmdbuffer** gfx_cmdbuffers;    // count: num_workers
    sx_tls tmp_allocs_tls;
    sx_tls cmdbuffer_tls;
    int num_workers;

    Remotery* rmt;
    rizz__core_cmd* console_cmds;    // sx_array

    rizz__tls_var* tls_vars;
} rizz__core;

static rizz__core g_core;
static rizz_api_imgui* the_imgui;

////////////////////////////////////////////////////////////////////////////////////////////////////
// Logging

#if SX_PLATFORM_WINDOWS
#    define EOL "\r\n"
#else
#    define EOL "\n"
#endif

static void rizz__log_to_file(const char* logfile, const char* text)
{
    FILE* f = fopen(logfile, "at");
    if (f) {
        fprintf(f, "%s%s", text, EOL);
        fclose(f);
    }
}

static void rizz__print_info(const char* fmt, ...)
{
    char text[1024];
    va_list args;
    va_start(args, fmt);
    sx_vsnprintf(text, sizeof(text), fmt, args);
    va_end(args);
    puts(text);

    if (g_core.flags & RIZZ_CORE_FLAG_LOG_TO_FILE)
        rizz__log_to_file(g_core.logfile, text);
    if (g_core.flags & RIZZ_CORE_FLAG_LOG_TO_PROFILER) {
        rmt_LogText(text);
    }

#if SX_COMPILER_MSVC && defined(_DEBUG)
    sx_strcat(text, sizeof(text), "\n");
    OutputDebugStringA(text);
#endif

#if SX_PLATFORM_ANDROID
    __android_log_write(ANDROID_LOG_INFO, g_core.app_name, text);
#endif
}

static void rizz__print_debug(const char* fmt, ...)
{
#ifdef _DEBUG
    char text[1024];
    char new_fmt[1024];

    sx_strcpy(new_fmt, sizeof(new_fmt), "DEBUG: ");
    sx_strcat(new_fmt, sizeof(new_fmt), fmt);

    va_list args;
    va_start(args, fmt);
    sx_vsnprintf(text, sizeof(text), new_fmt, args);
    va_end(args);
    printf("%s%s%s\n", TERM_COLOR_DIM, text, TERM_COLOR_RESET);

    if (g_core.flags & RIZZ_CORE_FLAG_LOG_TO_FILE)
        rizz__log_to_file(g_core.logfile, text);

    if (g_core.flags & RIZZ_CORE_FLAG_LOG_TO_PROFILER) {
        rmt_LogText(text);
    }

#    if SX_COMPILER_MSVC
    sx_strcat(text, sizeof(text), "\n");
    OutputDebugStringA(text);
#    endif

#    if SX_PLATFORM_ANDROID
    __android_log_write(ANDROID_LOG_DEBUG, g_core.app_name, text);
#    endif
#else
    sx_unused(fmt);
#endif
}

static void rizz__print_verbose(const char* fmt, ...)
{
    if (g_core.flags & RIZZ_CORE_FLAG_VERBOSE) {
        char text[1024];
        char new_fmt[1024];
        sx_strcpy(new_fmt, sizeof(new_fmt), "-- ");
        sx_strcat(new_fmt, sizeof(new_fmt), fmt);
        va_list args;
        va_start(args, fmt);
        sx_vsnprintf(text, sizeof(text), new_fmt, args);
        va_end(args);

        printf("%s%s%s\n", TERM_COLOR_DIM, text, TERM_COLOR_RESET);
        if (g_core.flags & RIZZ_CORE_FLAG_LOG_TO_FILE)
            rizz__log_to_file(g_core.logfile, text);
        if (g_core.flags & RIZZ_CORE_FLAG_LOG_TO_PROFILER) {
            rmt_LogText(text);
        }

#if SX_COMPILER_MSVC && defined(_DEBUG)
        sx_strcat(text, sizeof(text), "\n");
        OutputDebugStringA(text);
#endif

#if SX_PLATFORM_ANDROID
        __android_log_write(ANDROID_LOG_VERBOSE, g_core.app_name, text);
#endif
    }
}

static void rizz__print_error(const char* fmt, ...)
{
    char text[1024];
    char new_fmt[1024];
    sx_strcpy(new_fmt, sizeof(new_fmt), "ERROR: ");
    sx_strcat(new_fmt, sizeof(new_fmt), fmt);

    va_list args;
    va_start(args, fmt);
    sx_vsnprintf(text, sizeof(text), new_fmt, args);
    va_end(args);
    printf("%s%s%s\n", TERM_COLOR_RED, text, TERM_COLOR_RESET);

    if (g_core.flags & RIZZ_CORE_FLAG_LOG_TO_FILE)
        rizz__log_to_file(g_core.logfile, text);
    if (g_core.flags & RIZZ_CORE_FLAG_LOG_TO_PROFILER) {
        rmt_LogText(text);
    }
#if SX_COMPILER_MSVC && defined(_DEBUG)
    sx_strcat(text, sizeof(text), "\n");
    OutputDebugStringA(text);
#endif

#if SX_PLATFORM_ANDROID
    __android_log_write(ANDROID_LOG_ERROR, g_core.app_name, text);
#endif
}

static void rizz__print_error_trace(const char* source_file, int line, const char* fmt, ...)
{
    char text[1024];
    char new_fmt[1024];

#ifdef _DEBUG
    char basename[32];
    sx_os_path_basename(basename, sizeof(basename), source_file);
    sx_snprintf(new_fmt, sizeof(new_fmt), "ERROR: %s (%s, Line: %d)", fmt, basename, line);
#else
    sx_unused(source_file);
    sx_unused(line);

    sx_strcpy(new_fmt, sizeof(new_fmt), "ERROR: ");
    sx_strcat(new_fmt, sizeof(new_fmt), fmt);
#endif

    va_list args;
    va_start(args, fmt);
    sx_vsnprintf(text, sizeof(text), new_fmt, args);
    va_end(args);
    printf("%s%s%s\n", TERM_COLOR_RED, text, TERM_COLOR_RESET);

    if (g_core.flags & RIZZ_CORE_FLAG_LOG_TO_FILE)
        rizz__log_to_file(g_core.logfile, text);
    if (g_core.flags & RIZZ_CORE_FLAG_LOG_TO_PROFILER) {
        rmt_LogText(text);
    }

#if SX_COMPILER_MSVC && defined(_DEBUG)
    sx_strcat(text, sizeof(text), "\n");
    OutputDebugStringA(text);
#endif

#if SX_PLATFORM_ANDROID
    __android_log_write(ANDROID_LOG_ERROR, g_core.app_name, text);
#endif
}

static void rizz__print_warning(const char* fmt, ...)
{
    char text[1024];
    char new_fmt[1024];

    sx_strcpy(new_fmt, sizeof(new_fmt), "WARNING: ");
    sx_strcat(new_fmt, sizeof(new_fmt), fmt);

    va_list args;
    va_start(args, fmt);
    sx_vsnprintf(text, sizeof(text), new_fmt, args);
    va_end(args);
    printf("%s%s%s\n", TERM_COLOR_YELLOW, text, TERM_COLOR_RESET);

    if (g_core.flags & RIZZ_CORE_FLAG_LOG_TO_FILE)
        rizz__log_to_file(g_core.logfile, text);
    if (g_core.flags & RIZZ_CORE_FLAG_LOG_TO_PROFILER) {
        rmt_LogText(text);
    }

#if SX_COMPILER_MSVC && defined(_DEBUG)
    sx_strcat(text, sizeof(text), "\n");
    OutputDebugStringA(text);
#endif

#if SX_PLATFORM_ANDROID
    __android_log_write(ANDROID_LOG_WARN, g_core.app_name, text);
#endif
}
////////////////////////////////////////////////////////////////////////////////////////////////////

static void* rmt__malloc(void* ctx, uint32_t size)
{
    return sx_malloc((const sx_alloc*)ctx, size);
}

static void rmt__free(void* ctx, void* ptr)
{
    sx_free((const sx_alloc*)ctx, ptr);
}

static void* rmt__realloc(void* ctx, void* ptr, uint32_t size)
{
    return sx_realloc((const sx_alloc*)ctx, ptr, size);
}

static const sx_alloc* rizz__heap_alloc()
{
    return &g_core.heap_proxy_alloc;
}

static uint64_t rizz__delta_tick()
{
    return g_core.delta_tick;
}

static uint64_t rizz__elapsed_tick()
{
    return g_core.elapsed_tick;
}

static float rizz__fps()
{
    return g_core.fps_frame;
}

static float rizz__fps_mean()
{
    return g_core.fps_mean;
}

int64_t rizz__frame_index()
{
    return g_core.frame_idx;
}

void rizz__set_cache_dir(const char* path)
{
    sx_unused(path);
}

const char* rizz__cache_dir()
{
    return NULL;
}

const char* rizz__data_dir()
{
    return NULL;
}

sx_job_context* rizz__job_ctx()
{
    return g_core.jobs;
}

static const char* k__gfx_driver_names[RIZZ_GFX_BACKEND_DUMMY] = { "OpenGL 3.3",  "OpenGL-ES 2",
                                                                   "OpenGL-ES 3", "Direct3D11",
                                                                   "Metal IOS",   "Metal MacOS",
                                                                   "Metal Sim" };

static void rizz__init_log(const char* logfile)
{
    FILE* f = fopen(logfile, "wt");
    if (f) {
        time_t t = time(NULL);
        fprintf(f, "%s", asctime(localtime(&t)));
        fprintf(f, "%s: v%d.%d.%d - rizz v%d.%d.%d%s%s", g_core.app_name,
                rizz_version_major(g_core.app_ver), rizz_version_minor(g_core.app_ver),
                rizz_version_bugfix(g_core.app_ver), rizz_version_major(RIZZ_VERSION),
                rizz_version_minor(RIZZ_VERSION), rizz_version_bugfix(RIZZ_VERSION), EOL, EOL);
        fclose(f);
    } else {
        sx_assert(0 && "could not write to log file");
        g_core.flags &= ~RIZZ_CORE_FLAG_LOG_TO_FILE;
    }
}

static void rizz__job_thread_init_cb(sx_job_context* ctx, int thread_index, uint32_t thread_id,
                                     void* user)
{
    sx_unused(thread_id);
    sx_unused(user);
    sx_unused(ctx);

    int worker_index = thread_index + 1;    // 0 is rreserved for main-thread
    sx_tls_set(g_core.tmp_allocs_tls, &g_core.tmp_allocs[worker_index]);
    sx_tls_set(g_core.cmdbuffer_tls, g_core.gfx_cmdbuffers[worker_index]);

    char name[32];
    sx_snprintf(name, sizeof(name), "Thread #%d", worker_index);
    rmt_SetCurrentThreadName(name);
}

static void rizz__job_thread_shutdown_cb(sx_job_context* ctx, int thread_index, uint32_t thread_id,
                                         void* user)
{
    sx_unused(ctx);
    sx_unused(thread_index);
    sx_unused(thread_id);
    sx_unused(user);
}

static void rizz__rmt_input_handler(const char* text, void* context)
{
    sx_unused(context);

    // parse text into argc/argv
    int argc = 0;
    char arg[256];

    const char* cline = sx_skip_whitespace(text);
    while (cline && *cline) {
        for (char c = *cline; !sx_isspace(c) && *cline; c = *(++cline)) {
        }
        cline = sx_skip_whitespace(cline);
        argc++;
    }
    if (argc == 0)
        return;

    const sx_alloc* tmp_alloc = the__core.tmp_alloc_push();
    char** argv = sx_malloc(tmp_alloc, sizeof(char*) * argc);
    sx_assert(argv);

    cline = sx_skip_whitespace(text);
    int arg_idx = 0;
    while (cline && *cline) {
        int char_idx = 0;
        for (char c = *cline; !sx_isspace(c) && *cline && char_idx < (int)sizeof(arg);
             c = *(++cline), ++char_idx) {
            arg[char_idx] = c;
        }
        arg[char_idx] = '\0';
        cline = sx_skip_whitespace(cline);
        argv[arg_idx] = sx_malloc(tmp_alloc, (size_t)strlen(arg) + 1);
        sx_assert(argv);
        sx_strcpy(argv[arg_idx++], sizeof(arg), arg);
    }

    for (int i = 0; i < sx_array_count(g_core.console_cmds); i++) {
        if (sx_strequal(g_core.console_cmds[i].name, argv[0])) {
            int r;
            if ((r = g_core.console_cmds[i].callback(argc, argv)) < 0) {
                char err_msg[256];
                sx_snprintf(err_msg, sizeof(err_msg), "command '%s' failed with error: %d", argv[0],
                            r);
                rmt_LogText(err_msg);
            }
            break;
        }
    }

    the__core.tmp_alloc_pop();
}

static const sx_alloc* rizz__alloc(rizz_mem_id id)
{
#if RIZZ_CONFIG_DEBUG_MEMORY
    sx_assert(id < _RIZZ_MEMID_COUNT);
    return &g_core.track_allocs[id].alloc;
#else
    sx_unused(id);
    return &g_core.heap_proxy_alloc;
#endif
}

static void rizz__core_register_console_command(const char* cmd, rizz_core_cmd_cb* callback)
{
    sx_assert(cmd);
    sx_assert(cmd[0]);
    sx_assert(callback);

    rizz__core_cmd c;
    sx_strcpy(c.name, sizeof(c.name), cmd);
    c.callback = callback;
    sx_array_push(rizz__alloc(RIZZ_MEMID_CORE), g_core.console_cmds, c);
}

static inline void rizz__atomic_max(sx_atomic_size* _max, intptr_t val)
{
    intptr_t cur_max = *_max;
    while (cur_max < val && sx_atomic_cas_size(_max, val, cur_max) != cur_max) {
        cur_max = *_max;
    }
}

static void* rizz__proxy_alloc_cb(void* ptr, size_t size, uint32_t align, const char* file,
                                  const char* func, uint32_t line, void* user_data)
{
    sx_unused(ptr);

    const sx_alloc* heap_alloc = (const sx_alloc*)user_data;
    if (size == 0) {
        // free
        if (ptr) {
            uint8_t* aligned = (uint8_t*)ptr;
            rizz__proxy_alloc_header* header = (rizz__proxy_alloc_header*)ptr - 1;
            ptr = aligned - header->ptr_offset;

            sx_atomic_fetch_add_size(&g_core.heap_size, -header->size);
            sx_atomic_decr(&g_core.heap_count);

            sx__free(heap_alloc, ptr, 0, file, func, line);
        }

        return NULL;
    } else if (ptr == NULL) {
        // malloc
        align = sx_max((int)align, SX_CONFIG_ALLOCATOR_NATURAL_ALIGNMENT);

        // do the alignment ourselves
        intptr_t total = (intptr_t)size + sizeof(rizz__proxy_alloc_header) + align;
        uint8_t* _ptr = sx__malloc(heap_alloc, total, 0, file, func, line);
        if (!_ptr) {
            sx_out_of_memory();
            return NULL;
        }

        uint8_t* aligned = (uint8_t*)sx_align_ptr(_ptr, sizeof(rizz__proxy_alloc_header), align);
        rizz__proxy_alloc_header* header = (rizz__proxy_alloc_header*)aligned - 1;
        header->size = total;
        header->ptr_offset = (uint32_t)(uintptr_t)(aligned - _ptr);
        header->track_item_idx = -1;

        sx_atomic_fetch_add_size(&g_core.heap_size, total);
        rizz__atomic_max(&g_core.heap_max, g_core.heap_size);
        sx_atomic_incr(&g_core.heap_count);

        return aligned;
    } else {
        // realloc
        uint8_t* aligned = (uint8_t*)ptr;
        rizz__proxy_alloc_header* header = (rizz__proxy_alloc_header*)ptr - 1;
        uint32_t offset = header->ptr_offset;
        intptr_t prev_size = header->size;
        ptr = aligned - offset;
        sx_atomic_fetch_add_size(&g_core.heap_size, -header->size);

        align = sx_max((int)align, SX_CONFIG_ALLOCATOR_NATURAL_ALIGNMENT);
        intptr_t total = (intptr_t)size + sizeof(rizz__proxy_alloc_header) + align;
        ptr = sx__realloc(heap_alloc, ptr, total, 0, file, func, line);
        if (!ptr) {
            sx_out_of_memory();
            return NULL;
        }
        uint8_t* new_aligned = (uint8_t*)sx_align_ptr(ptr, sizeof(rizz__proxy_alloc_header), align);
        if (new_aligned == aligned) {
            header->size = total;
            return aligned;
        }

        aligned = (uint8_t*)ptr + offset;
        sx_memmove(new_aligned, aligned, size);
        header = (rizz__proxy_alloc_header*)new_aligned - 1;
        header->size = total;
        header->ptr_offset = (uint32_t)(uintptr_t)(new_aligned - (uint8_t*)ptr);

        intptr_t size_diff = total - prev_size;
        sx_atomic_fetch_add_size(&g_core.heap_size, size_diff);
        if (size_diff > 0)
            rizz__atomic_max(&g_core.heap_max, g_core.heap_size);
        return new_aligned;
    }
}

// NOTE: this special tracker alloc, always assumes that the redirecting allocator is
// proxy-allocator
//       Thus is assumes that all our pointers have rizz__proxy_alloc_header
// TODO: There is a catch and a possible bug:
//       malloc and realloc can conflict in multi-threaded environment, where it is called on the
//       same pointer on different threads
static void* rizz__track_alloc_cb(void* ptr, size_t size, uint32_t align, const char* file,
                                  const char* func, uint32_t line, void* user_data)
{

    rizz__track_alloc* talloc = user_data;
    const sx_alloc* proxy_alloc = &g_core.heap_proxy_alloc;

    if (size == 0) {
        // free
        if (ptr) {
            const rizz__proxy_alloc_header* header = (rizz__proxy_alloc_header*)ptr - 1;
            sx_lock(&talloc->_lk, 1);
            sx_assert(header->track_item_idx >= 0 &&
                      header->track_item_idx < sx_array_count(talloc->items));
            talloc->size -= header->size;
            rizz_track_alloc_item* mem_item = &talloc->items[header->track_item_idx];
            sx_assert(mem_item->ptr == ptr && "memory corruption");
            mem_item->ptr = NULL;    // invalidate memory item pointer

            if (header->track_item_idx != sx_array_count(talloc->items) - 1) {
                void* last_ptr = sx_array_last(talloc->items).ptr;
                sx_assert(last_ptr);
                rizz__proxy_alloc_header* last_header = (rizz__proxy_alloc_header*)last_ptr - 1;
                last_header->track_item_idx = header->track_item_idx;
                sx_array_pop(talloc->items, header->track_item_idx);
            } else {
                sx_array_pop_last(talloc->items);
            }
            sx_unlock(&talloc->_lk);
            sx__free(proxy_alloc, ptr, align, file, func, line);
        }

        return NULL;
    } else if (ptr == NULL) {
        // malloc
        ptr = sx__malloc(proxy_alloc, size, align, file, func, line);
        sx_assert(ptr);
        rizz__proxy_alloc_header* header = (rizz__proxy_alloc_header*)ptr - 1;
        rizz_track_alloc_item mem_item = { .ptr = ptr, .line = line, .size = header->size };
        sx_os_path_basename(mem_item.file, sizeof(mem_item.file), file);
        sx_strcpy(mem_item.func, sizeof(mem_item.func), func);

        sx_lock(&talloc->_lk, 1);
        talloc->size += header->size;
        talloc->peak = sx_max(talloc->peak, talloc->size);
        header->track_item_idx = sx_array_count(talloc->items);
        sx_array_push(g_core.heap_alloc, talloc->items, mem_item);
        sx_unlock(&talloc->_lk);

        return ptr;
    } else {
        // realloc
        const rizz__proxy_alloc_header* prev_header = (rizz__proxy_alloc_header*)ptr - 1;
        int64_t prev_size = prev_header->size;

        ptr = sx__realloc(proxy_alloc, ptr, size, align, file, func, line);
        sx_assert(ptr);
        const rizz__proxy_alloc_header* header = (rizz__proxy_alloc_header*)ptr - 1;

        sx_lock(&talloc->_lk, 1);
        sx_assert(header->track_item_idx >= 0 &&
                  header->track_item_idx < sx_array_count(talloc->items));
        int64_t size_diff = header->size - prev_size;
        talloc->size += size_diff;
        if (size_diff > 0)
            talloc->peak = sx_max(talloc->peak, talloc->size);
        rizz_track_alloc_item* mem_item = &talloc->items[header->track_item_idx];
        sx_os_path_basename(mem_item->file, sizeof(mem_item->file), file);
        sx_strcpy(mem_item->func, sizeof(mem_item->func), func);
        mem_item->ptr = ptr;
        mem_item->line = line;
        mem_item->size = header->size;
        sx_unlock(&talloc->_lk);

        return ptr;
    }
}

bool rizz__core_init(const rizz_config* conf)
{
#ifdef _DEBUG
    g_core.heap_alloc = sx_alloc_malloc_leak_detect();
#else
    g_core.heap_alloc = sx_alloc_malloc();
#endif

    if (RIZZ_CONFIG_DEBUG_MEMORY) {
        g_core.heap_proxy_alloc =
            (sx_alloc){ .alloc_cb = rizz__proxy_alloc_cb, .user_data = (void*)g_core.heap_alloc };

        // initialize track allocators
        for (int i = 0; i < _RIZZ_MEMID_COUNT; i++) {
            g_core.track_allocs[i] =
                (rizz__track_alloc){ .alloc = { .alloc_cb = rizz__track_alloc_cb,
                                                .user_data = (void*)&g_core.track_allocs[i] },
                                     .mem_id = (rizz_mem_id)i,
                                     .name = k__memid_names[i] };
        }
    } else {
        g_core.heap_proxy_alloc = *g_core.heap_alloc;
    }

    const sx_alloc* alloc = rizz__alloc(RIZZ_MEMID_CORE);
    sx_strcpy(g_core.app_name, sizeof(g_core.app_name), conf->app_name);
    g_core.app_ver = conf->app_version;
    g_core.flags = conf->core_flags;

#if SX_PLATFORM_ANDROID || SX_PLATFORM_IOS
    // remove log to file flag on mobile
    g_core.flags &= ~RIZZ_CORE_FLAG_LOG_TO_FILE;
#endif

    if (g_core.flags & RIZZ_CORE_FLAG_LOG_TO_FILE) {
        sx_strcpy(g_core.logfile, sizeof(g_core.logfile), conf->app_name);
        sx_strcat(g_core.logfile, sizeof(g_core.logfile), ".log");
        rizz__init_log(g_core.logfile);
    }

    sx_tm_init();
    sx_rng_seed(&g_core.rng, sizeof(time_t) == sizeof(uint64_t)
                                 ? sx_hash_u64_to_u32((uint64_t)time(NULL))
                                 : (uint32_t)time(NULL));

    // disk-io (virtual file system)
    if (!rizz__vfs_init(rizz__alloc(RIZZ_MEMID_VFS))) {
        rizz_log_error("initializing disk-io failed");
        return false;
    }
    rizz_log_info("(init) vfs");

    int num_worker_threads =
        conf->job_num_threads >= 0 ? conf->job_num_threads : (sx_os_numcores() - 1);
    num_worker_threads =
        sx_max(1, num_worker_threads);    // we should have at least one worker thread

    // Temp allocators
    g_core.num_workers = num_worker_threads + 1;    // include the main-thread
    g_core.tmp_allocs = sx_malloc(alloc, sizeof(rizz__core_tmpalloc) * g_core.num_workers);
    if (!g_core.tmp_allocs) {
        sx_out_of_memory();
        return false;
    }
    int tmp_size = (int)sx_align_mask(
        conf->tmp_mem_max > 0 ? conf->tmp_mem_max * 1024 : DEFAULT_TMP_SIZE, sx_os_pagesz() - 1);

    g_core.tmp_allocs_tls = sx_tls_create();
    sx_assert(g_core.tmp_allocs_tls);

    for (int i = 0; i < g_core.num_workers; i++) {
        rizz__core_tmpalloc* t = &g_core.tmp_allocs[i];
        void* mem = sx_virtual_commit(sx_virtual_reserve(tmp_size), tmp_size);
        if (!mem) {
            sx_out_of_memory();
            return false;
        }
        sx_stackalloc_init(&t->stack_alloc, mem, tmp_size);
        t->offset_stack = NULL;
    }
    sx_tls_set(g_core.tmp_allocs_tls, &g_core.tmp_allocs[0]);
    rizz_log_info("(init) temp memory: %dx%d kb", g_core.num_workers, tmp_size / 1024);

    // reflection
    if (!rizz__refl_init(rizz__alloc(RIZZ_MEMID_REFLECT), 0)) {
        rizz_log_error("initializing reflection failed");
        return false;
    }

    // asset system
#if SX_PLATFORM_ANDROID || SX_PLATFORM_IOS
    const char* asset_dbpath = "/assets/asset-db.json";
#else
    const char* asset_dbpath = "/cache/asset-db.json";
#endif
    if (!rizz__asset_init(rizz__alloc(RIZZ_MEMID_CORE), asset_dbpath, "")) {
        rizz_log_error("initializing asset system failed");
        return false;
    }
    rizz_log_info("(init) asset system: hot-loading=%d", RIZZ_CONFIG_HOT_LOADING);

    // profiler
    rmtSettings* rmt_config = rmt_Settings();
    if (rmt_config) {
        rmt_config->malloc = rmt__malloc;
        rmt_config->free = rmt__free;
        rmt_config->realloc = rmt__realloc;
        rmt_config->mm_context = (void*)rizz__alloc(RIZZ_MEMID_TOOLSET);
        rmt_config->port = conf->profiler_listen_port;
        rmt_config->msSleepBetweenServerUpdates = conf->profiler_update_interval_ms;
        rmt_config->reuse_open_port = true;
        rmt_config->input_handler = rizz__rmt_input_handler;
    }
    rmtError rmt_err;
    if ((rmt_err = rmt_CreateGlobalInstance(&g_core.rmt)) != RMT_ERROR_NONE) {
        rizz_log_warn("initializing profiler failed: %d", rmt_err);
    }

    rmt_SetCurrentThreadName("Main");
    if (RMT_ENABLED && g_core.rmt) {
        const char* profile_subsets;
        if (conf->core_flags & RIZZ_CORE_FLAG_PROFILE_GPU)
            profile_subsets = "cpu/gpu";
        else
            profile_subsets = "cpu";
        rizz_log_info("(init) profiler (%s): port=%d", profile_subsets, conf->profiler_listen_port);
    }

    // graphics
    sg_desc gfx_desc;
    rizz__app_init_gfx_desc(&gfx_desc);    // fill initial bindings for graphics/app
    // TODO:
    //  mtl_global_uniform_buffer_size
    //  mtl_sampler_cache_size
    // .buffer_pool_size:      128
    // .image_pool_size:       128
    // .shader_pool_size:      32
    // .pipeline_pool_size:    64
    // .pass_pool_size:        16
    // .context_pool_size:     16
    if (!rizz__gfx_init(rizz__alloc(RIZZ_MEMID_GRAPHICS), &gfx_desc,
                        (conf->core_flags & RIZZ_CORE_FLAG_PROFILE_GPU) ? true : false)) {
        rizz_log_error("initializing graphics failed");
        return false;
    }

    // create command buffers for each thread and their tls variable
    g_core.cmdbuffer_tls = sx_tls_create();
    sx_assert(g_core.cmdbuffer_tls);
    g_core.gfx_cmdbuffers = sx_malloc(alloc, sizeof(rizz_gfx_cmdbuffer*) * g_core.num_workers);
    if (!g_core.gfx_cmdbuffers) {
        sx_out_of_memory();
        return false;
    }
    for (int i = 0; i < g_core.num_workers; i++) {
        g_core.gfx_cmdbuffers[i] =
            rizz__gfx_create_command_buffer(rizz__alloc(RIZZ_MEMID_GRAPHICS));
        sx_assert(g_core.gfx_cmdbuffers[i]);
    }
    sx_tls_set(g_core.cmdbuffer_tls, g_core.gfx_cmdbuffers[0]);
    rizz_log_info("(init) graphics: %s", k__gfx_driver_names[the__gfx.backend()]);

    // job dispatcher
    g_core.jobs = sx_job_create_context(
        alloc, &(sx_job_context_desc){ .num_threads = num_worker_threads,
                                       .max_fibers = conf->job_max_fibers,
                                       .fiber_stack_sz = conf->job_stack_size * 1024,
                                       .thread_init_cb = rizz__job_thread_init_cb,
                                       .thread_shutdown_cb = rizz__job_thread_shutdown_cb });
    if (!g_core.jobs) {
        rizz_log_error("initializing job dispatcher failed");
        return false;
    }
    rizz_log_info("(init) jobs: threads=%d, max_fibers=%d, stack_size=%dkb",
                  sx_job_num_worker_threads(g_core.jobs), conf->job_max_fibers,
                  conf->job_stack_size);

    // coroutines
    g_core.coro =
        sx_coro_create_context(alloc, conf->coro_max_fibers, conf->coro_stack_size * 1024);
    if (!g_core.coro) {
        rizz_log_error("initializing coroutines failed");
        return false;
    }
    rizz_log_info("(init) coroutines: max_fibers=%d, stack_size=%dkb", conf->coro_max_fibers,
                  conf->coro_stack_size);

    // http client
    if (!rizz__http_init(alloc)) {
        rizz_log_error("initializing http failed");
        return false;
    }
    rizz_log_info("(init) http client");

    // Plugins
    if (!rizz__plugin_init(rizz__alloc(RIZZ_MEMID_CORE), conf->plugin_path)) {
        rizz_log_error("initializing plugins failed");
        return false;
    }

    // initialize cache-dir and load asset database
    the__vfs.mount(conf->cache_path, "/cache");
    if (!the__vfs.is_dir(conf->cache_path))
        the__vfs.mkdir(conf->cache_path);

    return true;
}

#ifdef _DEBUG
static void rizz__core_dump_leak(const char* formatted_msg, const char* file, const char* func,
                                 int line, size_t size, void* ptr)
{
    sx_unused(file);
    sx_unused(size);
    sx_unused(ptr);
    sx_unused(line);
    sx_unused(func);

    rizz_log_debug(formatted_msg);
}
#endif

void rizz__core_release()
{
    if (!g_core.heap_alloc)
        return;
    const sx_alloc* alloc = rizz__alloc(RIZZ_MEMID_CORE);

    // First release all plugins (+ game)
    rizz__plugin_release();

    // wait for all jobs to complete and destroy job manager
    // We place this here to let all plugins and game finish their work
    // The plugins are responsible for waiting on their jobs on shutdown
    if (g_core.jobs) {
        sx_job_destroy_context(g_core.jobs, alloc);
        g_core.jobs = NULL;
    }

    if (g_core.coro)
        sx_coro_destroy_context(g_core.coro, alloc);

    // destroy gfx command-buffers (per-thread)
    for (int i = 0; i < g_core.num_workers; i++) {
        rizz__gfx_destroy_command_buffer(g_core.gfx_cmdbuffers[i]);
    }
    sx_free(alloc, g_core.gfx_cmdbuffers);

#if !SX_PLATFORM_ANDROID && !SX_PLATFORM_IOS
    rizz__asset_save_meta_cache();
#endif

    if (g_core.flags & RIZZ_CORE_FLAG_DUMP_UNUSED_ASSETS)
        rizz__asset_dump_unused("unused-assets.json");

    // Release native subsystems
    rizz__http_release();
    rizz__asset_release();
    rizz__gfx_release();
    rizz__vfs_release();
    rizz__refl_release();

    if (g_core.rmt) {
        rmt_DestroyGlobalInstance(g_core.rmt);
    }
    sx_array_free(alloc, g_core.console_cmds);

    if (g_core.tmp_allocs) {
        for (int i = 0; i < g_core.num_workers; i++) {
            void* mem = g_core.tmp_allocs[i].stack_alloc.ptr;
            if (mem)
                sx_virtual_release(mem, g_core.tmp_allocs[i].stack_alloc.size);
            sx_array_free(alloc, g_core.tmp_allocs[i].offset_stack);
        }
        sx_free(alloc, g_core.tmp_allocs);
    }

    if (g_core.tmp_allocs_tls)
        sx_tls_destroy(g_core.tmp_allocs_tls);
    if (g_core.cmdbuffer_tls)
        sx_tls_destroy(g_core.cmdbuffer_tls);

    if (RIZZ_CONFIG_DEBUG_MEMORY) {
        for (int i = 0; i < _RIZZ_MEMID_COUNT; i++) {
            sx_array_free(g_core.heap_alloc, g_core.track_allocs[i].items);
        }
    }

    for (int i = 0; i < sx_array_count(g_core.tls_vars); i++) {
        if (g_core.tls_vars[i].tls) {
            sx_tls_destroy(g_core.tls_vars[i].tls);
        }
    }
    sx_array_free(&g_core.heap_proxy_alloc, g_core.tls_vars);

    rizz_log_info("shutdown");

#ifdef _DEBUG
    sx_dump_leaks(rizz__core_dump_leak);
#endif
    sx_memset(&g_core, 0x0, sizeof(g_core));
}

void rizz__core_frame()
{
    // Measure timing and fps
    g_core.delta_tick = sx_tm_laptime(&g_core.last_tick);
    g_core.elapsed_tick += g_core.delta_tick;

    uint64_t delta_tick = g_core.delta_tick;
    float dt = (float)sx_tm_sec(delta_tick);

    if (delta_tick > 0) {
        double afps = g_core.fps_mean;
        double fps = 1.0 / dt;

        afps += (fps - afps) / (double)g_core.frame_idx;
        g_core.fps_mean = (float)afps;
        g_core.fps_frame = (float)fps;
    }

    // submit render commands to the gpu
    // currently, we are submitting the calls from the previous frame
    // because submitting commands
    rizz__gfx_trace_reset_frame_stats();
    rizz__gfx_execute_command_buffers();

    // commit render (metal only)
    rizz__gfx_commit();

    // reset temp allocators
    for (int i = 0, c = g_core.num_workers; i < c; i++) {
        sx_stackalloc_reset(&g_core.tmp_allocs[i].stack_alloc);
    }

    // update internal sub-systems
    rizz__http_update();
    rizz__vfs_async_update();
    rizz__asset_update();
    rizz__gfx_update();
    sx_coro_update(g_core.coro, dt);

    // update plugins and application
    rizz__plugin_update(dt);

    the_imgui = the__plugin.get_api_byname("imgui", 0);
    if (the_imgui) {
        the_imgui->Render();
    }

    ++g_core.frame_idx;
}

static const sx_alloc* rizz__core_tmp_alloc_push()
{
    rizz__core_tmpalloc* talloc = sx_tls_get(g_core.tmp_allocs_tls);
    sx_array_push(rizz__alloc(RIZZ_MEMID_CORE), talloc->offset_stack, talloc->stack_alloc.offset);
    return &talloc->stack_alloc.alloc;
}

static void rizz__core_tmp_alloc_pop()
{
    rizz__core_tmpalloc* talloc = sx_tls_get(g_core.tmp_allocs_tls);
    if (sx_array_count(talloc->offset_stack)) {
        int last_offset = sx_array_last(talloc->offset_stack);
        sx_array_pop_last(talloc->offset_stack);
        talloc->stack_alloc.offset = last_offset;
        talloc->stack_alloc.last_ptr_offset = 0;
    }
}

rizz_gfx_cmdbuffer* rizz__core_gfx_cmdbuffer()
{
    return (rizz_gfx_cmdbuffer*)sx_tls_get(g_core.cmdbuffer_tls);
}

static uint32_t rizz__rand()
{
    return sx_rng_gen(&g_core.rng);
}

static float rizz__randf()
{
    return sx_rng_gen_f(&g_core.rng);
}

static int rizz__rand_range(int _min, int _max)
{
    return sx_rng_gen_irange(&g_core.rng, _min, _max);
}

static sx_job_t rizz__job_dispatch(int count,
                                   void (*callback)(int start, int end, int thrd_index, void* user),
                                   void* user, sx_job_priority priority, uint32_t tags)
{
    sx_assert(g_core.jobs);
    return sx_job_dispatch(g_core.jobs, count, callback, user, priority, tags);
}

static void rizz__job_wait_and_del(sx_job_t job)
{
    sx_assert(g_core.jobs);
    sx_job_wait_and_del(g_core.jobs, job);
}

static bool rizz__job_test_and_del(sx_job_t job)
{
    sx_assert(g_core.jobs);
    return sx_job_test_and_del(g_core.jobs, job);
}

static int rizz__job_num_workers()
{
    return g_core.num_workers;
}

static void rizz__begin_profile_sample(const char* name, uint32_t flags, uint32_t* hash_cache)
{
    sx_unused(name);
    sx_unused(flags);
    sx_unused(hash_cache);
    rmt__begin_cpu_sample(name, flags, hash_cache);
}

static void rizz__end_profile_sample()
{
    rmt__end_cpu_sample();
}

static void rizz__get_mem_info(rizz_mem_info* info)
{
    for (int i = 0; i < _RIZZ_MEMID_COUNT; i++) {
        rizz__track_alloc* t = &g_core.track_allocs[i];
        info->trackers[i] = (rizz_trackalloc_info){ .name = t->name,
                                                    .items = t->items,
                                                    .num_items = sx_array_count(t->items),
                                                    .mem_id = t->mem_id,
                                                    .size = t->size,
                                                    .peak = t->peak };
    }

    int num_temp_allocs = sx_min(g_core.num_workers, RIZZ_MAX_TEMP_ALLOCS);
    for (int i = 0; i < g_core.num_workers && i < num_temp_allocs; i++) {
        info->temp_allocs[i] =
            (rizz_linalloc_info){ .offset = g_core.tmp_allocs[i].stack_alloc.offset,
                                  .size = g_core.tmp_allocs[i].stack_alloc.size,
                                  .peak = g_core.tmp_allocs[i].stack_alloc.peak };
    }

    info->num_temp_allocs = g_core.num_workers;
    info->heap = g_core.heap_size;
    info->heap_max = g_core.heap_max;
    info->heap_count = g_core.heap_count;
}

static void rizz__coro_invoke(void (*coro_cb)(sx_fiber_transfer), void* user)
{
    sx__coro_invoke(g_core.coro, coro_cb, user);
}

static void rizz__coro_end(void* pfrom)
{
    sx__coro_end(g_core.coro, pfrom);
}

static void rizz__coro_wait(void* pfrom, int msecs)
{
    sx__coro_wait(g_core.coro, pfrom, msecs);
}

static void rizz__coro_yield(void* pfrom, int nframes)
{
    sx__coro_yield(g_core.coro, pfrom, nframes);
}

void rizz__core_fix_callback_ptrs(const void** ptrs, const void** new_ptrs, int num_ptrs)
{
    for (int i = 0; i < num_ptrs; i++) {
        if (ptrs[i] && ptrs[i] != new_ptrs[i] &&
            sx_coro_replace_callback(g_core.coro, (sx_fiber_cb*)ptrs[i],
                                     (sx_fiber_cb*)new_ptrs[i])) {
            rizz_log_warn("coroutine 0x%p replaced with 0x%p and restarted!", ptrs[i], new_ptrs[i]);
        }
    }
}

static void rizz__core_tls_register(const char* name, void* user,
                                    void* (*init_cb)(int thread_idx, uint32_t thread_id,
                                                     void* user))
{
    sx_assert(name);
    sx_assert(init_cb);

    rizz__tls_var tvar = (rizz__tls_var){ .name_hash = sx_hash_fnv32_str(name),
                                          .user = user,
                                          .init_cb = init_cb,
                                          .tls = sx_tls_create() };

    sx_assert(tvar.tls);

    sx_array_push(&g_core.heap_proxy_alloc, g_core.tls_vars, tvar);
}

static void* rizz__core_tls_var(const char* name)
{
    sx_assert(name);
    uint32_t hash = sx_hash_fnv32_str(name);
    for (int i = 0, c = sx_array_count(g_core.tls_vars); i < c; i++) {
        rizz__tls_var* tvar = &g_core.tls_vars[i];

        if (tvar->name_hash == hash) {
            void* var = sx_tls_get(tvar->tls);
            if (!var) {
                var = tvar->init_cb(sx_job_thread_index(g_core.jobs), sx_job_thread_id(g_core.jobs),
                                    tvar->user);
                sx_tls_set(tvar->tls, var);
            }
            return var;
        }
    }

    sx_assert(0 && "tls_var not registered");
    return NULL;
}

// Core API
rizz_api_core the__core = { .heap_alloc = rizz__heap_alloc,
                            .tmp_alloc_push = rizz__core_tmp_alloc_push,
                            .tmp_alloc_pop = rizz__core_tmp_alloc_pop,
                            .tls_register = rizz__core_tls_register,
                            .tls_var = rizz__core_tls_var,
                            .alloc = rizz__alloc,
                            .get_mem_info = rizz__get_mem_info,
                            .rand = rizz__rand,
                            .randf = rizz__randf,
                            .rand_range = rizz__rand_range,
                            .delta_tick = rizz__delta_tick,
                            .elapsed_tick = rizz__elapsed_tick,
                            .fps = rizz__fps,
                            .fps_mean = rizz__fps_mean,
                            .frame_index = rizz__frame_index,
                            .set_cache_dir = rizz__set_cache_dir,
                            .cache_dir = rizz__cache_dir,
                            .data_dir = rizz__data_dir,
                            .job_dispatch = rizz__job_dispatch,
                            .job_wait_and_del = rizz__job_wait_and_del,
                            .job_test_and_del = rizz__job_test_and_del,
                            .job_num_workers = rizz__job_num_workers,
                            .coro_invoke = rizz__coro_invoke,
                            .coro_end = rizz__coro_end,
                            .coro_wait = rizz__coro_wait,
                            .coro_yield = rizz__coro_yield,
                            .print_info = rizz__print_info,
                            .print_debug = rizz__print_debug,
                            .print_verbose = rizz__print_verbose,
                            .print_error_trace = rizz__print_error_trace,
                            .print_error = rizz__print_error,
                            .print_warning = rizz__print_warning,
                            .begin_profile_sample = rizz__begin_profile_sample,
                            .end_profile_sample = rizz__end_profile_sample,
                            .register_console_command = rizz__core_register_console_command };
