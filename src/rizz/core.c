//
// Copyright 2019 Sepehr Taghdisian (septag@github). All rights reserved.
// License: https://github.com/septag/rizz#license-bsd-2-clause
//
#include "internal.h"

#include "rizz/imgui.h"
#include "rizz/imgui-extra.h"

#include "rizz/rizz.h"
#include "sx/allocator.h"
#include "sx/array.h"
#include "sx/atomic.h"
#include "sx/hash.h"
#include "sx/jobs.h"
#include "sx/lockless.h"
#include "sx/os.h"
#include "sx/rng.h"
#include "sx/stack-alloc.h"
#include "sx/string.h"
#include "sx/threads.h"
#include "sx/timer.h"
#include "sx/vmem.h"

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

// cj5 implementation, external modules (non-static build) should implement it for themselves
#define CJ5_ASSERT(e)		sx_assert(e);
#define CJ5_IMPLEMENT
#include "cj5/cj5.h"

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

static const char* k__memid_names[_RIZZ_MEMID_COUNT] = { "Core",          //
                                                         "Graphics",      //
                                                         "Audio",         //
                                                         "IO",            //
                                                         "Reflection",    //
                                                         "Other",         //
                                                         "Debug",         //
                                                         "Toolset",       //
                                                         "Input",         //
                                                         "Game" };


typedef struct rizz__core_tmpalloc {
    sx_alloc alloc;
    sx_vmem_context vmem;
    sx_stackalloc stack_alloc;
    size_t* offset_stack;    // sx_array - keep offsets in a stack for push()/pop()
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

typedef struct rizz__log_backend {
    char name[32];
    void* user;
    void (*log_cb)(const rizz_log_entry* entry, void* user);
} rizz__log_backend;

typedef struct rizz__log_entry_internal {
    rizz_log_entry e;
    sx_str_t text_id;
    sx_str_t source_id;
    uint64_t timestamp;
} rizz__log_entry_internal;

typedef struct rizz__log_entry_internal_ref {
    rizz__log_entry_internal e;
    int pipe_idx;
} rizz__log_entry_internal_ref;

typedef struct rizz__log_pipe {
    sx_queue_spsc* queue;    // item_type = rizz__log_entry_internal
    sx_strpool* strpool;     //
} rizz__log_pipe;

typedef struct rizz__show_debugger_deferred {
    bool show;
    bool* p_open;
} rizz__show_debugger_deferred;

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
    int num_threads;

    int64_t frame_idx;
    int64_t frame_stats_reset;  // frame index that draw stats was reset
    uint64_t elapsed_tick;
    uint64_t delta_tick;
    uint64_t last_tick;
    float fps_mean;
    float fps_frame;

    rizz_version ver;
    uint32_t app_ver;
    char app_name[32];
    char logfile[32];
    rizz_log_level log_level;

    rizz__core_tmpalloc* tmp_allocs;    // count: num_threads
    rizz__log_pipe* log_pipes;          // count: num_threads

    Remotery* rmt;
    rizz__core_cmd* console_cmds;       // sx_array
    rizz__log_backend* log_backends;    // sx_array
    rizz__tls_var* tls_vars;            // sx_array
    sx_atomic_int num_log_backends;

    rizz__show_debugger_deferred show_memory;
    rizz__show_debugger_deferred show_graphics;
    rizz__show_debugger_deferred show_log;
} rizz__core;

#define SORT_NAME log__sort_entries
#define SORT_TYPE rizz__log_entry_internal_ref
#define SORT_CMP(x, y) ((x).e.timestamp < (y).e.timestamp ? -1 : 1)
SX_PRAGMA_DIAGNOSTIC_PUSH()
SX_PRAGMA_DIAGNOSTIC_IGNORED_MSVC(4267)
SX_PRAGMA_DIAGNOSTIC_IGNORED_MSVC(4244)
SX_PRAGMA_DIAGNOSTIC_IGNORED_MSVC(4146)
SX_PRAGMA_DIAGNOSTIC_IGNORED_CLANG_GCC("-Wunused-function")
SX_PRAGMA_DIAGNOSTIC_IGNORED_CLANG("-Wshorten-64-to-32")
#include "sort/sort.h"
SX_PRAGMA_DIAGNOSTIC_POP()

static rizz__core g_core;

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

////////////////////////////////////////////////////////////////////////////////////////////////////
// @log
#if SX_PLATFORM_WINDOWS
#    define EOL "\r\n"
#else
#    define EOL "\n"
#endif

static const char* k_log_entry_types[_RIZZ_LOG_LEVEL_COUNT] = { "ERROR: ",      //
                                                                "WARNING: ",    //
                                                                "",             //
                                                                "VERBOSE: ",    //
                                                                "DEBUG: " };

static bool rizz__parse_version(const char* version_str, int* major, int* minor, char* git,
                                int git_size)
{
    if (version_str[0] != 'v') {
        return false;
    }

    ++version_str;
    
    char num[32];
    const char* end_major = sx_strchar(version_str, '.');
    if (!end_major) {
        return false;
    }
     sx_strncpy(num, sizeof(num), version_str, (int)(intptr_t)(end_major - version_str));
    *major = sx_toint(num);

    const char* end_minor = sx_strchar(end_major + 1, '-');
    if (!end_minor) {
        return false;
    }
    sx_strncpy(num, sizeof(num), end_major + 1, (int)(intptr_t)(end_minor - end_major - 1));
    *minor = sx_toint(num);

    sx_strcpy(git, git_size, end_minor + 1);

    return true;
}

static void rizz__log_register_backend(const char* name,
                                       void (*log_cb)(const rizz_log_entry* entry, void* user),
                                       void* user)
{    // backend name must be unique
    for (int i = 0, c = sx_array_count(g_core.log_backends); i < c; i++) {
        if (sx_strequal(g_core.log_backends[i].name, name)) {
            sx_assert_rel(0 && "duplicate backend name/already registered?");
            return;
        }
    }

    rizz__log_backend backend = { .user = user, .log_cb = log_cb };
    sx_strcpy(backend.name, sizeof(backend.name), name);
    sx_array_push(rizz__alloc(RIZZ_MEMID_CORE), g_core.log_backends, backend);

    sx_atomic_incr(&g_core.num_log_backends);
}

static void rizz__log_unregister_backend(const char* name)
{
    for (int i = 0, c = sx_array_count(g_core.log_backends); i < c; i++) {
        if (sx_strequal(g_core.log_backends[i].name, name)) {
            sx_array_pop(g_core.log_backends, i);
            sx_atomic_decr(&g_core.num_log_backends);
            return;
        }
    }
}

static void rizz__log_make_source_str(char* text, int text_size, const char* file, int line)
{
    if (file) {
        char filename[64];
        sx_os_path_basename(filename, sizeof(filename), file);
        sx_snprintf(text, text_size, "%s(%d): ", filename, line);
    } else {
        text[0] = '\0';
    }
}

static void rizz__log_make_source_str_full(char* text, int text_size, const char* file, int line)
{
    if (file) {
        sx_snprintf(text, text_size, "%s(%d): ", file, line);
    } else {
        text[0] = '\0';
    }
}

static void rizz__log_backend_debugger(const rizz_log_entry* entry, void* user)
{
    sx_unused(user);

#if SX_COMPILER_MSVC
    int new_size = entry->text_len + 128;
    char* text = alloca(new_size);

    if (text) {
        char source[128];
        rizz__log_make_source_str_full(source, sizeof(source), entry->source_file, entry->line);
        sx_snprintf(text, new_size, "%s%s%s\n", source, k_log_entry_types[entry->type],
                    entry->text);
        OutputDebugStringA(text);
    } else {
        sx_assert_rel(0 && "out of stack memory");
    }
#else
    sx_unused(entry);
#endif
}

static void rizz__log_backend_terminal(const rizz_log_entry* entry, void* user)
{
    sx_unused(user);

    int new_size = entry->text_len + 128;
    char* text = alloca(new_size);

    if (text) {
        const char* open_fmt;
        const char* close_fmt;

        // terminal coloring
        // clang-format off
		switch (entry->type) {
        case RIZZ_LOG_LEVEL_INFO:		open_fmt = "";  close_fmt = "";	break;
        case RIZZ_LOG_LEVEL_DEBUG:		open_fmt = TERM_COLOR_DIM; close_fmt = TERM_COLOR_RESET; break;
        case RIZZ_LOG_LEVEL_VERBOSE:	open_fmt = TERM_COLOR_DIM; close_fmt = TERM_COLOR_RESET; break;
        case RIZZ_LOG_LEVEL_WARNING:	open_fmt = TERM_COLOR_RED; close_fmt = TERM_COLOR_RESET; break;
        case RIZZ_LOG_LEVEL_ERROR:		open_fmt = TERM_COLOR_YELLOW; close_fmt = TERM_COLOR_RESET; break;
		default:			    		open_fmt = "";	close_fmt = "";	break;
        }
        // clang-format on

        sx_snprintf(text, new_size, "%s%s%s%s", open_fmt, k_log_entry_types[entry->type],
                    entry->text, close_fmt);
        puts(text);
    } else {
        sx_assert_rel(0 && "out of stack memory");
    }
}

// TODO: backend should only just open the file once per frame 
static void rizz__log_backend_file(const rizz_log_entry* entry, void* user)
{
    sx_unused(user);

    char source[128];
    rizz__log_make_source_str(source, sizeof(source), entry->source_file, entry->line);

    FILE* f = fopen(g_core.logfile, "at");
    if (f) {
        fprintf(f, "%s%s%s\n", source, k_log_entry_types[entry->type], entry->text);
        fclose(f);
    }
}

#if SX_PLATFORM_ANDROID
static void rizz__log_backend_android(const rizz_log_entry* entry, void* user)
{
    sx_unused(user);

    int new_size = entry->text_len + 128;
    char* text = alloca(new_size);

    if (text) {
        char source[128];
        rizz__log_make_source_str(source, sizeof(source), entry->source_file, entry->line);

        // clang-format off
		enum android_LogPriority apriority;
		switch (entry->type) {
		case RIZZ_LOG_LEVEL_INFO:		apriority = ANDROID_LOG_INFO;		break;
		case RIZZ_LOG_LEVEL_DEBUG:		apriority = ANDROID_LOG_DEBUG;		break;
		case RIZZ_LOG_LEVEL_VERBOSE:	apriority = ANDROID_LOG_VERBOSE;	break;
		case RIZZ_LOG_LEVEL_WARNING:	apriority = ANDROID_LOG_ERROR;		break;
		case RIZZ_LOG_LEVEL_ERROR:		apriority = ANDROID_LOG_WARN;		break;
		default:						apriority = 0;						break;
		}
        // clang-format on

        sx_snprintf(text, new_size, "%s: %s", source, entry->text);
        __android_log_write(apriority, g_core.app_name, text);
    } else {
        sx_assert_rel(0 && "out of stack memory");
    }
}
#endif // SX_PLATFORM_ANDROID

static void rizz__log_backend_remotery(const rizz_log_entry* entry, void* user)
{
    sx_unused(user);

    int new_size = entry->text_len + 128;
    char* text = alloca(new_size);

    if (text) {
        char source[128];
        rizz__log_make_source_str(source, sizeof(source), entry->source_file, entry->line);

        sx_snprintf(text, new_size, "%s%s%s", source, k_log_entry_types[entry->type], entry->text);
        rmt_LogText(text);
    } else {
        sx_assert_rel(0 && "out of stack memory");
    }
}

static void rizz__log_dispatch_entry(const rizz_log_entry* entry)
{
    
    // built-in backends are thread-safe, so we pass them immediately
    rizz__log_backend_terminal(entry, NULL);
    rizz__log_backend_debugger(entry, NULL);

#if SX_PLATFORM_ANDROID
    rizz__log_backend_android(entry, NULL);
#endif

    if (g_core.num_log_backends > 0) {
        rizz__log_pipe pipe =
            g_core.jobs ? g_core.log_pipes[sx_job_thread_index(g_core.jobs)] : g_core.log_pipes[0];

        sx_str_t text = sx_strpool_add(pipe.strpool, entry->text, entry->text_len);
        sx_str_t source = entry->source_file ? sx_strpool_add(pipe.strpool, entry->source_file,
                                                              entry->source_file_len)
                                             : 0;
        rizz__log_entry_internal entry_internal = { .e = *entry,
                                                    .text_id = text,
                                                    .source_id = source,
                                                    .timestamp = sx_cycle_clock() };
        sx_queue_spsc_produce_and_grow(pipe.queue, &entry_internal, rizz__alloc(RIZZ_MEMID_CORE));
    }
}

static void rizz__set_log_level(rizz_log_level level)
{
    g_core.log_level = level;
}

static void rizz__print_info(uint32_t channels, const char* source_file, int line, const char* fmt,
                             ...)
{
    if (g_core.log_level < RIZZ_LOG_LEVEL_INFO) {
        return;
    }

    int fmt_len = sx_strlen(fmt);
    char* text = alloca(fmt_len + 1024);    // reserve only 1k for format replace strings
    if (!text) {
        sx_assert_rel(0 && "out of stack memory");
        return;
    }

    va_list args;
    va_start(args, fmt);
    sx_vsnprintf(text, fmt_len + 1024, fmt, args);
    va_end(args);

    rizz__log_dispatch_entry(&(rizz_log_entry){ .type = RIZZ_LOG_LEVEL_INFO,
                                                .channels = channels,
                                                .text_len = sx_strlen(text),
                                                .source_file_len = source_file ? sx_strlen(source_file) : 0,
                                                .text = text,
                                                .source_file = source_file,
                                                .line = line });
}

static void rizz__print_debug(uint32_t channels, const char* source_file, int line, const char* fmt,
                              ...)
{
#ifdef _DEBUG
    if (g_core.log_level < RIZZ_LOG_LEVEL_DEBUG) {
        return;
    }

    int fmt_len = sx_strlen(fmt);
    char* text = alloca(fmt_len + 1024);    // reserve only 1k for format replace strings
    if (!text) {
        sx_assert_rel(0 && "out of stack memory");
        return;
    }

    va_list args;
    va_start(args, fmt);
    sx_vsnprintf(text, fmt_len + 1024, fmt, args);
    va_end(args);

    rizz__log_dispatch_entry(&(rizz_log_entry){ .type = RIZZ_LOG_LEVEL_DEBUG,
                                                .channels = channels,
                                                .text_len = sx_strlen(text),
                                                .source_file_len = source_file ? sx_strlen(source_file) : 0,
                                                .text = text,
                                                .source_file = source_file,
                                                .line = line });
#else   // if _DEBUG
    sx_unused(channels);
    sx_unused(source_file);
    sx_unused(line);
    sx_unused(fmt);
#endif  // else _DEBUG
}

static void rizz__print_verbose(uint32_t channels, const char* source_file, int line,
                                const char* fmt, ...)
{
    if (g_core.log_level < RIZZ_LOG_LEVEL_VERBOSE) {
        return;
    }

    int fmt_len = sx_strlen(fmt);
    char* text = alloca(fmt_len + 1024);    // reserve only 1k for format replace strings
    if (!text) {
        sx_assert_rel(0 && "out of stack memory");
        return;
    }

    va_list args;
    va_start(args, fmt);
    sx_vsnprintf(text, fmt_len + 1024, fmt, args);
    va_end(args);

    rizz__log_dispatch_entry(&(rizz_log_entry){ .type = RIZZ_LOG_LEVEL_VERBOSE,
                                                .channels = channels,
                                                .text_len = sx_strlen(text),
                                                .source_file_len = source_file ? sx_strlen(source_file) : 0,
                                                .text = text,
                                                .source_file = source_file,
                                                .line = line });
}

static void rizz__print_error(uint32_t channels, const char* source_file, int line, const char* fmt,
                              ...)
{
    if (g_core.log_level < RIZZ_LOG_LEVEL_ERROR) {
        return;
    }

    int fmt_len = sx_strlen(fmt);
    char* text = alloca(fmt_len + 1024);    // reserve only 1k for format replace strings
    if (!text) {
        sx_assert_rel(0 && "out of stack memory");
        return;
    }

    va_list args;
    va_start(args, fmt);
    sx_vsnprintf(text, fmt_len + 1024, fmt, args);
    va_end(args);

    rizz__log_dispatch_entry(&(rizz_log_entry){ .type = RIZZ_LOG_LEVEL_ERROR,
                                                .channels = channels,
                                                .text_len = sx_strlen(text),
                                                .source_file_len = source_file ? sx_strlen(source_file) : 0,
                                                .text = text,
                                                .source_file = source_file,
                                                .line = line });
}

static void rizz__print_warning(uint32_t channels, const char* source_file, int line,
                                const char* fmt, ...)
{
    if (g_core.log_level < RIZZ_LOG_LEVEL_WARNING) {
        return;
    }

    int fmt_len = sx_strlen(fmt);
    char* text = alloca(fmt_len + 1024);    // reserve only 1k for format replace strings
    if (!text) {
        sx_assert_rel(0 && "out of stack memory");
        return;
    }

    va_list args;
    va_start(args, fmt);
    sx_vsnprintf(text, fmt_len + 1024, fmt, args);
    va_end(args);

    rizz__log_dispatch_entry(&(rizz_log_entry){ .type = RIZZ_LOG_LEVEL_WARNING,
                                                .channels = channels,
                                                .text_len = sx_strlen(text),
                                                .source_file_len = source_file ? sx_strlen(source_file) : 0,
                                                .text = text,
                                                .source_file = source_file,
                                                .line = line });
}

static void rizz__log_update()
{
    const sx_alloc* tmp_alloc = the__core.tmp_alloc_push();
    rizz__log_entry_internal_ref* entries = NULL;
    
    // collect all log entries from threads and sort them by timestamp
    for (int ti = 0, tc = g_core.num_threads; ti < tc; ti++) {
        rizz__log_pipe pipe = g_core.log_pipes[ti];
        rizz__log_entry_internal_ref entry;
        entry.pipe_idx = ti;
        while (sx_queue_spsc_consume(pipe.queue, &entry.e)) {
            entry.e.e.text = sx_strpool_cstr(pipe.strpool, entry.e.text_id);
            entry.e.e.source_file =
                entry.e.source_id ? sx_strpool_cstr(pipe.strpool, entry.e.source_id) : NULL;
            sx_array_push(tmp_alloc, entries, entry);
        }
    } // foreach thread
    
    int num_entries = sx_array_count(entries);
    if (num_entries > 1) {
        log__sort_entries_tim_sort(entries, num_entries);
    }

    if (num_entries > 0) {
        for (int i = 0, c = sx_array_count(g_core.log_backends); i < c; i++) {
            const rizz__log_backend* backend = &g_core.log_backends[i];

            for (int ei = 0; ei < num_entries; ei++) {
                rizz__log_entry_internal_ref entry = entries[ei];
                rizz__log_pipe pipe = g_core.log_pipes[entry.pipe_idx];

                backend->log_cb(&entry.e.e, backend->user);

                if (entry.e.text_id) {
                    sx_strpool_del(pipe.strpool, entry.e.text_id);
                }
                if (entry.e.source_id) {
                    sx_strpool_del(pipe.strpool, entry.e.source_id);
                }
            }    // foreach entry
        } // foreach backend
    }
    
    the__core.tmp_alloc_pop();
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

static const sx_alloc* rizz__heap_alloc(void)
{
    return &g_core.heap_proxy_alloc;
}

static uint64_t rizz__delta_tick(void)
{
    return g_core.delta_tick;
}

static float rizz__delta_time(void)
{
    return (float)sx_tm_sec(g_core.delta_tick);
}

static uint64_t rizz__elapsed_tick(void)
{
    return g_core.elapsed_tick;
}

static float rizz__fps(void)
{
    return g_core.fps_frame;
}

static float rizz__fps_mean(void)
{
    return g_core.fps_mean;
}

int64_t rizz__frame_index(void)
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

static void rizz__log_init_file(const char* logfile)
{
    FILE* f = fopen(logfile, "wt");
    if (f) {
        time_t t = time(NULL);
        fprintf(f, "%s", asctime(localtime(&t)));
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

    char name[32];
    sx_snprintf(name, sizeof(name), "Thread #%d", thread_index + 1 /* 0 is for main-thread */);
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

    const sx_alloc* alloc = the__core.alloc(RIZZ_MEMID_CORE);
    char** argv = sx_malloc(alloc, sizeof(char*) * argc);
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
        argv[arg_idx] = sx_malloc(alloc, (size_t)strlen(arg) + 1);
        sx_assert(argv);
        sx_strcpy(argv[arg_idx++], sizeof(arg), arg);
    }

    bool cmd_found = false;
    for (int i = 0; i < sx_array_count(g_core.console_cmds); i++) {
        if (sx_strequal(g_core.console_cmds[i].name, argv[0])) {
            cmd_found = true;
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

    if (!cmd_found) {
        char msg[256];
        sx_snprintf(msg, sizeof(msg), "command '%s' not found", argv[0]);
        rmt_LogText(msg);
    }
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

static void* rizz__tmp_alloc_cb(void* ptr, size_t size, uint32_t align, const char* file,
                                  const char* func, uint32_t line, void* user_data)
{
    rizz__core_tmpalloc* alloc = user_data;
    size_t raw_size = sx_stackalloc_real_alloc_size(size, align);
    if ((raw_size + alloc->stack_alloc.offset) > alloc->stack_alloc.size) {
        // maximum reached, extend it by allocating new pages
        // size_t size_needed = raw_size + alloc->stack_alloc.offset - alloc->stack_alloc.size;
        // int num_pages = (int)(size_needed + alloc->vmem.page_size - 1) / alloc->vmem.page_size;
        int num_pages = (int)(raw_size + alloc->vmem.page_size - 1) / alloc->vmem.page_size;
        void* page_ptr = sx_vmem_commit_pages(&alloc->vmem, alloc->vmem.num_pages, num_pages);
        if (page_ptr == NULL) {
            sx_out_of_memory();
            return NULL;
        }
        alloc->stack_alloc.size = sx_vmem_commit_size(&alloc->vmem);
        // align offset to nearest page
        size_t new_offset = (uintptr_t)page_ptr - (uintptr_t)alloc->vmem.ptr;
        sx_assert(new_offset % alloc->vmem.page_size == 0);
        alloc->stack_alloc.offset = new_offset;
    }

    return sx__realloc(&alloc->stack_alloc.alloc, ptr, size, align, file, func, line);
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
            sx_lock(&talloc->_lk);
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
        if (file) {
            sx_os_path_basename(mem_item.file, sizeof(mem_item.file), file);
        }
        if (func) {
            sx_strcpy(mem_item.func, sizeof(mem_item.func), func);
        }

        sx_lock(&talloc->_lk);
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

        sx_lock(&talloc->_lk);
        sx_assert(header->track_item_idx >= 0 &&
                  header->track_item_idx < sx_array_count(talloc->items));
        int64_t size_diff = header->size - prev_size;
        talloc->size += size_diff;
        if (size_diff > 0)
            talloc->peak = sx_max(talloc->peak, talloc->size);
        rizz_track_alloc_item* mem_item = &talloc->items[header->track_item_idx];
        if (file) {
            sx_os_path_basename(mem_item->file, sizeof(mem_item->file), file);
        }
        if (func) {
            sx_strcpy(mem_item->func, sizeof(mem_item->func), func);
        }
        mem_item->ptr = ptr;
        mem_item->line = line;
        mem_item->size = header->size;
        sx_unlock(&talloc->_lk);

        return ptr;
    }
}

bool rizz__core_init(const rizz_config* conf)
{
    g_core.heap_alloc = (conf->core_flags & RIZZ_CORE_FLAG_DETECT_LEAKS)
                            ? sx_alloc_malloc_leak_detect()
                            : sx_alloc_malloc();

#ifdef RIZZ_VERSION
    rizz__parse_version(sx_stringize(RIZZ_VERSION), &g_core.ver.major, &g_core.ver.minor, 
                        g_core.ver.git, sizeof(g_core.ver.git));
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
    g_core.log_level = conf->log_level;

    int num_worker_threads =
        conf->job_num_threads >= 0 ? conf->job_num_threads : (sx_os_numcores() - 1);
    num_worker_threads =
        sx_max(1, num_worker_threads);              // we should have at least one worker thread
    g_core.num_threads = num_worker_threads + 1;    // include the main-thread

    // log queues per thread
    g_core.log_pipes = sx_malloc(alloc, sizeof(rizz__log_pipe) * g_core.num_threads);
    if (!g_core.log_pipes) {
        sx_out_of_memory();
        return false;
    }
    for (int i = 0; i < g_core.num_threads; i++) {
        g_core.log_pipes[i].queue =
            sx_queue_spsc_create(alloc, sizeof(rizz__log_entry_internal), 32);
        g_core.log_pipes[i].strpool = sx_strpool_create(alloc, NULL);
        if (!g_core.log_pipes[i].queue || !g_core.log_pipes[i].strpool) {
            sx_out_of_memory();
            return false;
        }
    }

#if SX_PLATFORM_ANDROID || SX_PLATFORM_IOS
    // TEMP: remove log to file flag on mobile
    g_core.flags &= ~RIZZ_CORE_FLAG_LOG_TO_FILE;
#endif

    if (g_core.flags & RIZZ_CORE_FLAG_LOG_TO_FILE) {
        sx_strcpy(g_core.logfile, sizeof(g_core.logfile), conf->app_name);
        sx_strcat(g_core.logfile, sizeof(g_core.logfile), ".log");
        rizz__log_init_file(g_core.logfile);
        rizz__log_register_backend("file", rizz__log_backend_file, NULL);
    }

    if (g_core.flags & RIZZ_CORE_FLAG_LOG_TO_PROFILER) {
        rizz__log_register_backend("remotery", rizz__log_backend_remotery, NULL);
    }

    // log version
    rizz__log_info("version: %d.%d-%s", g_core.ver.major, g_core.ver.minor, g_core.ver.git);

    sx_tm_init();
    sx_rng_seed(&g_core.rng, sizeof(time_t) == sizeof(uint64_t)
                                 ? sx_hash_u64_to_u32((uint64_t)time(NULL))
                                 : (uint32_t)time(NULL));

    // disk-io (virtual file system)
    if (!rizz__vfs_init(rizz__alloc(RIZZ_MEMID_VFS))) {
        rizz__log_error("initializing disk-io failed");
        return false;
    }
    rizz__log_info("(init) vfs");

    // Temp allocators
    {
        g_core.tmp_allocs = sx_malloc(alloc, sizeof(rizz__core_tmpalloc) * g_core.num_threads);
        if (!g_core.tmp_allocs) {
            sx_out_of_memory();
            return false;
        }
        size_t page_sz = sx_os_pagesz();
        size_t tmp_size = sx_align_mask(
            conf->tmp_mem_max > 0 ? conf->tmp_mem_max * 1024 : DEFAULT_TMP_SIZE, page_sz - 1);
        int num_tmp_pages =  sx_vmem_get_needed_pages(tmp_size);

        for (int i = 0; i < g_core.num_threads; i++) {
            rizz__core_tmpalloc* t = &g_core.tmp_allocs[i];
            t->alloc = (sx_alloc) {
                .alloc_cb = rizz__tmp_alloc_cb,
                .user_data = t
            };
            if (!sx_vmem_init(&t->vmem, 0, num_tmp_pages)) {
                sx_out_of_memory();
                return false;
            }
            sx_vmem_commit_page(&t->vmem, 0);
            sx_stackalloc_init(&t->stack_alloc, t->vmem.ptr, page_sz);
            t->offset_stack = NULL;
        }
        rizz__log_info("(init) temp memory: %dx%d kb", g_core.num_threads, tmp_size / 1024);
    }

    // job dispatcher
    g_core.jobs = sx_job_create_context(
        alloc, &(sx_job_context_desc){ .num_threads = num_worker_threads,
                                       .max_fibers = conf->job_max_fibers,
                                       .fiber_stack_sz = conf->job_stack_size * 1024,
                                       .thread_init_cb = rizz__job_thread_init_cb,
                                       .thread_shutdown_cb = rizz__job_thread_shutdown_cb });
    if (!g_core.jobs) {
        rizz__log_error("initializing job dispatcher failed");
        return false;
    }
    rizz__log_info("(init) jobs: threads=%d, max_fibers=%d, stack_size=%dkb",
                   sx_job_num_worker_threads(g_core.jobs), conf->job_max_fibers,
                   conf->job_stack_size);

    // reflection
    if (!rizz__refl_init(rizz__alloc(RIZZ_MEMID_REFLECT), 0)) {
        rizz__log_error("initializing reflection failed");
        return false;
    }

    // asset system
#if SX_PLATFORM_ANDROID || SX_PLATFORM_IOS
    const char* asset_dbpath = "/assets/asset-db.json";
#else
    const char* asset_dbpath = "/cache/asset-db.json";
#endif
    if (!rizz__asset_init(rizz__alloc(RIZZ_MEMID_CORE), asset_dbpath, "")) {
        rizz__log_error("initializing asset system failed");
        return false;
    }
    rizz__log_info("(init) asset system: hot-loading=%d", RIZZ_CONFIG_HOT_LOADING);

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
        rizz__log_warn("initializing profiler failed: %d", rmt_err);
    }

    rmt_SetCurrentThreadName("Main");
    if (RMT_ENABLED && g_core.rmt) {
        const char* profile_subsets;
        if (conf->core_flags & RIZZ_CORE_FLAG_PROFILE_GPU)
            profile_subsets = "cpu/gpu";
        else
            profile_subsets = "cpu";
        rizz__log_info("(init) profiler (%s): port=%d", profile_subsets,
                       conf->profiler_listen_port);
    }

    // graphics
    sg_desc gfx_desc;
    rizz__app_init_gfx_desc(&gfx_desc);    // fill initial bindings for graphics/app
    // TODO: override these default values with config
    //  mtl_global_uniform_buffer_size
    //  mtl_sampler_cache_size
    // .buffer_pool_size:      128
    // .image_pool_size:       128
    // .shader_pool_size:      32
    // .pipeline_pool_size:    64
    // .pass_pool_size:        16
    // .context_pool_size:     16
    // .sampler_cache_size     64
    // .uniform_buffer_size    4 MB (4*1024*1024)
    // .staging_buffer_size    8 MB (8*1024*1024)
    // .context.color_format
    // .context.depth_format
    gfx_desc.context.sample_count = conf->multisample_count;
    if (!rizz__gfx_init(rizz__alloc(RIZZ_MEMID_GRAPHICS), &gfx_desc,
                        (conf->core_flags & RIZZ_CORE_FLAG_PROFILE_GPU) ? true : false)) {
        rizz__log_error("initializing graphics failed");
        return false;
    }

    rizz__log_info("(init) graphics: %s", k__gfx_driver_names[the__gfx.backend()]);

    // coroutines
    g_core.coro =
        sx_coro_create_context(alloc, conf->coro_max_fibers, conf->coro_stack_size * 1024);
    if (!g_core.coro) {
        rizz__log_error("initializing coroutines failed");
        return false;
    }
    rizz__log_info("(init) coroutines: max_fibers=%d, stack_size=%dkb", conf->coro_max_fibers,
                   conf->coro_stack_size);

    // http client
    if (!rizz__http_init(alloc)) {
        rizz__log_error("initializing http failed");
        return false;
    }
    rizz__log_info("(init) http client");

    // Plugins
    if (!rizz__plugin_init(rizz__alloc(RIZZ_MEMID_CORE), conf->plugin_path)) {
        rizz__log_error("initializing plugins failed");
        return false;
    }

    // initialize cache-dir and load asset database
    the__vfs.mount(conf->cache_path, "/cache");
    if (!the__vfs.is_dir(conf->cache_path)) {
        the__vfs.mkdir(conf->cache_path);
    }

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

    rizz__log_debug(formatted_msg);
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

    if (g_core.coro) {
        sx_coro_destroy_context(g_core.coro, alloc);
    }

    if (g_core.flags & RIZZ_CORE_FLAG_DUMP_UNUSED_ASSETS) {
        rizz__asset_dump_unused("unused-assets.txt");
    }

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
        for (int i = 0; i < g_core.num_threads; i++) {
            sx_vmem_release(&g_core.tmp_allocs[i].vmem);
            sx_array_free(alloc, g_core.tmp_allocs[i].offset_stack);
        }
        sx_free(alloc, g_core.tmp_allocs);
    }

    // release log backends and queues
    for (int i = 0; i < g_core.num_threads; i++) {
        if (g_core.log_pipes[i].queue) {
            sx_queue_spsc_destroy(g_core.log_pipes[i].queue, alloc);
        }
        if (g_core.log_pipes[i].strpool) {
            sx_strpool_destroy(g_core.log_pipes[i].strpool, alloc);
        }
    }
    sx_free(alloc, g_core.log_pipes);
    sx_array_free(alloc, g_core.log_backends);
    g_core.num_log_backends = 0;

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

    rizz__log_info("shutdown");

#ifdef _DEBUG
    sx_dump_leaks(rizz__core_dump_leak);
#endif

    sx_memset(&g_core, 0x0, sizeof(g_core));
}

void rizz__core_frame()
{
    rizz__profile_begin(FRAME, 0);
    {
        static uint32_t gpu_frame_hash = 0;
        the__gfx.imm.begin_profile_sample("FRAME", &gpu_frame_hash);
    }

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

    // reset temp allocators
    size_t page_sz = sx_os_pagesz();
    for (int i = 0, c = g_core.num_threads; i < c; i++) {
        rizz__core_tmpalloc* t = &g_core.tmp_allocs[i];
        sx_vmem_free_pages(&t->vmem, 1, t->vmem.num_pages - 1);
        sx_stackalloc_reset(&t->stack_alloc);
        t->stack_alloc.size = page_sz;
    }

    rizz__gfx_trace_reset_frame_stats(RIZZ_GFX_TRACE_COMMON);

    // update internal sub-systems
    rizz__http_update();
    rizz__vfs_async_update();
    rizz__asset_update();
    rizz__gfx_update();

    rizz__profile_begin(Coroutines, 0);
    sx_coro_update(g_core.coro, dt);
    rizz__profile_end(Coroutines);

    // update plugins and application
    rizz__plugin_update(dt);

    // execute remaining commands from the 'staged' API
    rizz__profile_begin(Execute_command_buffers, 0);
    rizz__gfx_execute_command_buffers_final();
    rizz__profile_end(Execute_command_buffers);

    // flush queued logs
    rizz__profile_begin(Log_update, 0);
    rizz__log_update();
    rizz__profile_end(Log_update);

    // draw imgui stuff
    rizz_api_imgui* the_imgui = the__plugin.get_api_byname("imgui", 0);
    if (the_imgui) {
        rizz__profile_begin(ImGui_draw, 0);
        rizz_api_imgui_extra* the_imguix = the__plugin.get_api_byname("imgui_extra", 0);
        if (g_core.show_memory.show) {
            rizz_mem_info minfo;
            the__core.get_mem_info(&minfo);
            the_imguix->memory_debugger(&minfo, g_core.show_memory.p_open);
            g_core.show_memory.show = false;
        }
        if (g_core.show_graphics.show) {
            the_imguix->graphics_debugger(the__gfx.trace_info(), g_core.show_graphics.p_open);
            g_core.show_graphics.show = false;
        }
        if (g_core.show_log.show) {
            the_imguix->show_log(g_core.show_log.p_open);
            g_core.show_log.show = false;
        }

        rizz__gfx_trace_reset_frame_stats(RIZZ_GFX_TRACE_IMGUI);
        the_imgui->Render();
        rizz__profile_end(ImGui_draw);
    }

    rizz__gfx_commit_gpu();
    ++g_core.frame_idx;

    the__gfx.imm.end_profile_sample();
    rizz__profile_end(FRAME);
}

static const sx_alloc* rizz__core_tmp_alloc_push()
{
    rizz__core_tmpalloc* talloc = &g_core.tmp_allocs[sx_job_thread_index(g_core.jobs)];
    sx_array_push(rizz__alloc(RIZZ_MEMID_CORE), talloc->offset_stack, talloc->stack_alloc.offset);
    return &talloc->alloc;
}

static void rizz__core_tmp_alloc_pop()
{
    rizz__core_tmpalloc* talloc = &g_core.tmp_allocs[sx_job_thread_index(g_core.jobs)];
    if (sx_array_count(talloc->offset_stack)) {
        size_t last_offset = sx_array_last(talloc->offset_stack);
        sx_array_pop_last(talloc->offset_stack);
        talloc->stack_alloc.offset = last_offset;
        talloc->stack_alloc.last_ptr_offset = 0;
    }
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

static int rizz__job_num_threads()
{
    return g_core.num_threads;
}

static int rizz__job_thread_index()
{
    sx_assert(g_core.jobs);
    return sx_job_thread_index(g_core.jobs);
}

static void rizz__begin_profile_sample(const char* name, rizz_profile_flags flags, uint32_t* hash_cache)
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

    int num_temp_allocs = sx_min(g_core.num_threads, RIZZ_MAX_TEMP_ALLOCS);
    for (int i = 0; i < g_core.num_threads && i < num_temp_allocs; i++) {
        info->temp_allocs[i] =
            (rizz_linalloc_info){ .offset = g_core.tmp_allocs[i].stack_alloc.offset,
                                  .size = g_core.tmp_allocs[i].stack_alloc.size,
                                  .peak = g_core.tmp_allocs[i].stack_alloc.peak };
    }

    info->num_trackers = RIZZ_CONFIG_DEBUG_MEMORY ? _RIZZ_MEMID_COUNT : 0;
    info->num_temp_allocs = g_core.num_threads;
    info->heap = g_core.heap_size;
    info->heap_max = g_core.heap_max;
    info->heap_count = g_core.heap_count;
}

static void rizz__core_coro_invoke(void (*coro_cb)(sx_fiber_transfer), void* user)
{
    sx__coro_invoke(g_core.coro, coro_cb, user);
}

static void rizz__core_coro_end(void* pfrom)
{
    sx__coro_end(g_core.coro, pfrom);
}

static void rizz__core_coro_wait(void* pfrom, int msecs)
{
    sx__coro_wait(g_core.coro, pfrom, msecs);
}

static void rizz__core_coro_yield(void* pfrom, int nframes)
{
    sx__coro_yield(g_core.coro, pfrom, nframes);
}

void rizz__core_fix_callback_ptrs(const void** ptrs, const void** new_ptrs, int num_ptrs)
{
    for (int i = 0; i < num_ptrs; i++) {
        if (ptrs[i] && ptrs[i] != new_ptrs[i] &&
            sx_coro_replace_callback(g_core.coro, (sx_fiber_cb*)ptrs[i],
                                     (sx_fiber_cb*)new_ptrs[i])) {
            rizz__log_warn("coroutine 0x%p replaced with 0x%p and restarted!", ptrs[i],
                           new_ptrs[i]);
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

static rizz_version rizz__version(void)
{
    return g_core.ver;
}

static void rizz__show_graphics_debugger(bool* p_open) 
{
    g_core.show_graphics.show = true;
    g_core.show_graphics.p_open = p_open;
}

static void rizz__show_memory_debugger(bool* p_open)
{
    g_core.show_memory.show = true;
    g_core.show_memory.p_open = p_open;
}

static void rizz__show_log(bool* p_open)
{
    g_core.show_log.show = true;
    g_core.show_log.p_open = p_open;
}

// Core API
rizz_api_core the__core = { .heap_alloc = rizz__heap_alloc,
                            .tmp_alloc_push = rizz__core_tmp_alloc_push,
                            .tmp_alloc_pop = rizz__core_tmp_alloc_pop,
                            .tls_register = rizz__core_tls_register,
                            .tls_var = rizz__core_tls_var,
                            .alloc = rizz__alloc,
                            .get_mem_info = rizz__get_mem_info,
                            .version = rizz__version,
                            .rand = rizz__rand,
                            .randf = rizz__randf,
                            .rand_range = rizz__rand_range,
                            .delta_tick = rizz__delta_tick,
                            .delta_time = rizz__delta_time,
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
                            .job_num_threads = rizz__job_num_threads,
                            .job_thread_index = rizz__job_thread_index,
                            .coro_invoke = rizz__core_coro_invoke,
                            .coro_end = rizz__core_coro_end,
                            .coro_wait = rizz__core_coro_wait,
                            .coro_yield = rizz__core_coro_yield,
                            .register_log_backend = rizz__log_register_backend,
                            .unregister_log_backend = rizz__log_unregister_backend,
                            .print_info = rizz__print_info,
                            .print_debug = rizz__print_debug,
                            .print_verbose = rizz__print_verbose,
                            .print_error = rizz__print_error,
                            .print_warning = rizz__print_warning,
                            .set_log_level = rizz__set_log_level,
                            .begin_profile_sample = rizz__begin_profile_sample,
                            .end_profile_sample = rizz__end_profile_sample,
                            .register_console_command = rizz__core_register_console_command,
                            .show_graphics_debugger = rizz__show_graphics_debugger,
                            .show_memory_debugger = rizz__show_memory_debugger,
                            .show_log = rizz__show_log };
