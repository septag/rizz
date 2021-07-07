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
#include "sx/macros.h"
#include "sx/os.h"
#include "sx/string.h"
#include "sx/threads.h"
#include "sx/timer.h"
#include "sx/vmem.h"
#include "sx/pool.h"

#include <alloca.h>
#include <stdio.h>
#include <time.h>

#include "Remotery.h"

#define MAX_TEMP_ALLOC_WAIT_TIME 2.0
#define RMT_SMALL_MEMORY_SIZE 160

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
#define CJ5_ASSERT(e)		sx_assert(e)
#define CJ5_IMPLEMENT
#include "cj5/cj5.h"

#define DEFAULT_TMP_SIZE    0xA00000    // 10mb

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


typedef struct rizz__core_tmpalloc rizz__core_tmpalloc;
typedef struct rizz__core_tmpalloc_inst {
    sx_alloc alloc;
    rizz__core_tmpalloc* parent;      // pointer to parent rizz__core_tmpalloc
    size_t end_offset;
    size_t start_offset;
    size_t start_lastptr_offset;
    int depth;
    const char* file;
    uint32_t line;
} rizz__core_tmpalloc_inst;

typedef struct rizz__core_tmpalloc {
    sx_vmem_context vmem;
    rizz__core_tmpalloc_inst* alloc_stack;    // sx_array - stack for push()/pop()
    sx_atomic_int stack_depth;
    float wait_time;
    size_t peak;
    size_t frame_peak;
} rizz__core_tmpalloc;

typedef struct rizz__core_tmpalloc_debug_item {
    void* ptr;
    size_t size;
    char file[32];
    uint32_t line;
} rizz__core_tmpalloc_debug_item;

typedef struct rizz__core_tmpalloc_debug rizz__core_tmpalloc_debug;
typedef struct rizz__core_tmpalloc_debug_inst {
    sx_alloc alloc;
    rizz__core_tmpalloc_debug* owner;
    int item_idx;
    const char* file;
    uint32_t line;
} rizz__core_tmpalloc_debug_inst;

// debug temp-allocator is a replacement for tmpalloc, which allocates from heap instead of linear alloc
typedef struct rizz__core_tmpalloc_debug {
    size_t offset;
    size_t max_size;
    size_t peak;
    size_t frame_peak;
    sx_atomic_int stack_depth;
    float wait_time;
    rizz__core_tmpalloc_debug_item* items;          // sx_array: allocated items within 
    rizz__core_tmpalloc_debug_inst* alloc_stack;    // sx_array - stack for push()/pop() api
} rizz__core_tmpalloc_debug;

typedef struct rizz__core_cmd {
    char name[32];
    rizz_core_cmd_cb* callback;
    void* user;
} rizz__core_cmd;

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
    sx_alloc* track_allocs[_RIZZ_MEMID_COUNT];

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

    rizz__core_tmpalloc* tmp_allocs;                // count: num_threads
    rizz__core_tmpalloc_debug* tmp_debug_allocs;    // count: num_threads
    rizz__log_pipe* log_pipes;                      // count: num_threads

    Remotery* rmt;
    sx_queue_spsc* rmt_command_queue;   // type: char*, producer: remotery thread, consumer: main thread
    
    rizz__core_cmd* console_cmds;       // sx_array
    rizz__log_backend* log_backends;    // sx_array
    rizz__tls_var* tls_vars;            // sx_array
    sx_atomic_int num_log_backends;

    rizz__show_debugger_deferred show_memory;
    rizz__show_debugger_deferred show_graphics;
    rizz__show_debugger_deferred show_log;
    
    sx_mutex rmt_mtx;
    sx_pool* rmt_alloc_pool;
    sx_strpool* strpool;

    bool paused;
} rizz__core;

#define SORT_NAME log__sort_entries
#define SORT_TYPE rizz__log_entry_internal_ref
#define SORT_CMP(x, y) ((x).e.timestamp < (y).e.timestamp ? -1 : 1)
SX_PRAGMA_DIAGNOSTIC_PUSH()
SX_PRAGMA_DIAGNOSTIC_IGNORED_MSVC(4267)
SX_PRAGMA_DIAGNOSTIC_IGNORED_MSVC(4244)
SX_PRAGMA_DIAGNOSTIC_IGNORED_MSVC(4146)
SX_PRAGMA_DIAGNOSTIC_IGNORED_MSVC(4505)
SX_PRAGMA_DIAGNOSTIC_IGNORED_CLANG_GCC("-Wunused-function")
SX_PRAGMA_DIAGNOSTIC_IGNORED_CLANG("-Wshorten-64-to-32")
#include "sort/sort.h"
SX_PRAGMA_DIAGNOSTIC_POP()

static rizz__core g_core;

static const sx_alloc* rizz__alloc(rizz_mem_id id)
{
    #if RIZZ_CONFIG_DEBUG_MEMORY
        sx_assert(id < _RIZZ_MEMID_COUNT);
        return g_core.track_allocs[id];
    #else
        sx_unused(id);
        return &g_core.heap_alloc;
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
            sx_assert_alwaysf(0, "duplicate backend name/already registered?");
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
        sx_assert_alwaysf(0, "out of stack memory");
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
		switch (entry->type) {
        case RIZZ_LOG_LEVEL_INFO:		open_fmt = "";  close_fmt = "";	break;
        case RIZZ_LOG_LEVEL_DEBUG:		open_fmt = TERM_COLOR_DIM; close_fmt = TERM_COLOR_RESET; break;
        case RIZZ_LOG_LEVEL_VERBOSE:	open_fmt = TERM_COLOR_DIM; close_fmt = TERM_COLOR_RESET; break;
        case RIZZ_LOG_LEVEL_WARNING:	open_fmt = TERM_COLOR_YELLOW; close_fmt = TERM_COLOR_RESET; break;
        case RIZZ_LOG_LEVEL_ERROR:		open_fmt = TERM_COLOR_RED; close_fmt = TERM_COLOR_RESET; break;
		default:			    		open_fmt = "";	close_fmt = "";	break;
        }

        sx_snprintf(text, new_size, "%s%s%s%s", open_fmt, k_log_entry_types[entry->type],
                    entry->text, close_fmt);
        puts(text);
    } else {
        sx_assert_alwaysf(0, "out of stack memory");
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
        sx_assert_always(0 && "out of stack memory");
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
        sx_assert_alwaysf(0, "out of stack memory");
    }
}

static void rizz__log_dispatch_entry(rizz_log_entry* entry)
{
    // built-in backends are thread-safe, so we pass them immediately
    rizz__log_backend_terminal(entry, NULL);
    rizz__log_backend_debugger(entry, NULL);

#if SX_PLATFORM_ANDROID
    rizz__log_backend_android(entry, NULL);
#endif

    if (g_core.num_log_backends > 0) {
        if (entry->channels == 0) {
            entry->channels = 0xffffffff;
        }

        rizz__log_pipe pipe =
            g_core.jobs ? g_core.log_pipes[sx_job_thread_index(g_core.jobs)] : g_core.log_pipes[0];

        sx_str_t text = sx_strpool_add(pipe.strpool, entry->text, entry->text_len);
        sx_str_t source = entry->source_file ? sx_strpool_add(pipe.strpool, entry->source_file,
                                                              entry->source_file_len) : 0;
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

static void rizz__print_info(uint32_t channels, const char* source_file, int line, const char* fmt, ...)
{
    if (g_core.log_level < RIZZ_LOG_LEVEL_INFO) {
        return;
    }

    int fmt_len = sx_strlen(fmt);
    char* text = alloca(fmt_len + 1024);    // reserve only 1k for format replace strings
    if (!text) {
        sx_assert_alwaysf(0, "out of stack memory");
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

static void rizz__print_debug(uint32_t channels, const char* source_file, int line, const char* fmt, ...)
{
#ifdef _DEBUG
    if (g_core.log_level < RIZZ_LOG_LEVEL_DEBUG) {
        return;
    }

    int fmt_len = sx_strlen(fmt);
    char* text = alloca(fmt_len + 1024);    // reserve only 1k for format replace strings
    if (!text) {
        sx_assert_alwaysf(0, "out of stack memory");
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

static void rizz__print_verbose(uint32_t channels, const char* source_file, int line, const char* fmt, ...)
{
    if (g_core.log_level < RIZZ_LOG_LEVEL_VERBOSE) {
        return;
    }

    int fmt_len = sx_strlen(fmt);
    char* text = alloca(fmt_len + 1024);    // reserve only 1k for format replace strings
    if (!text) {
        sx_assert_alwaysf(0, "out of stack memory");
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

static void rizz__print_error(uint32_t channels, const char* source_file, int line, const char* fmt, ...)
{
    if (g_core.log_level < RIZZ_LOG_LEVEL_ERROR) {
        return;
    }

    int fmt_len = sx_strlen(fmt);
    char* text = alloca(fmt_len + 1024);    // reserve only 1k for format replace strings
    if (!text) {
        sx_assert_alwaysf(0, "out of stack memory");
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
        sx_assert_alwaysf(0, "out of stack memory");
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
    rizz__with_temp_alloc(tmp_alloc) {
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
    }  // tmp_alloc
}

////////////////////////////////////////////////////////////////////////////////////////////////////
static void* rmt__malloc(void* ctx, uint32_t size)
{
    const sx_alloc* fallback_alloc = (const sx_alloc*)ctx;
    if (size <= RMT_SMALL_MEMORY_SIZE) {
        sx_mutex_enter(&g_core.rmt_mtx);
        void* ptr = sx_pool_new_and_grow(g_core.rmt_alloc_pool, fallback_alloc);
        sx_mutex_exit(&g_core.rmt_mtx);
        return ptr;
    } else {
        return sx_malloc(fallback_alloc, size);
    }
}

static void rmt__free(void* ctx, void* ptr)
{
    const sx_alloc* fallback_alloc = (const sx_alloc*)ctx;
    if (ptr) {
        if (sx_pool_valid_ptr(g_core.rmt_alloc_pool, ptr)) {
            sx_mutex_lock(g_core.rmt_mtx) {
                sx_pool_del(g_core.rmt_alloc_pool, ptr);
            }
            sx_mutex_exit(&g_core.rmt_mtx);
        } else {
            sx_free(fallback_alloc, ptr);
        }
    }
}

static void* rmt__realloc(void* ctx, void* ptr, uint32_t size)
{
    const sx_alloc* fallback_alloc = (const sx_alloc*)ctx;
    if (sx_pool_valid_ptr(g_core.rmt_alloc_pool, ptr)) {
        sx_mutex_lock(g_core.rmt_mtx) {
            sx_pool_del(g_core.rmt_alloc_pool, ptr);
        }
    } else {
        sx_free(fallback_alloc, ptr);
    }
    
    return rmt__malloc(ctx, size);
}

static const sx_alloc* rizz__heap_alloc(void)
{
    return g_core.heap_alloc;
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
        sx_assertf(0, "could not write to log file");
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
    const sx_alloc* alloc = context;
    int len = sx_strlen(text);
    if (len > 0) {
        char* str = sx_malloc(alloc, len + 1);
        if (str) {
            sx_memcpy(str, text, len + 1);
            sx_queue_spsc_produce(g_core.rmt_command_queue, (const void*)&str);
        }
    }
}

static void rizz__rmt_read_string(sx_mem_reader* r, char* str, uint32_t size)
{
    uint32_t len;
    uint32_t max_len = size - 1;
    sx_mem_read(r, &len, sizeof(len));
    sx_mem_read(r, str, sx_min(max_len, len));
    str[len] = '\0';
}

static void rizz__rmt_read_sample(sx_mem_reader* r)
{
    uint32_t name_hash;
    uint32_t unique_id;
    uint8_t unique_id_html_color[7];
    double us_start;
    double us_length;
    double us_self;
    uint32_t num_calls;
    uint32_t max_recurse_depth;
    sx_mem_read(r, &name_hash, sizeof(name_hash));
    sx_mem_read(r, &unique_id, sizeof(unique_id));
    sx_mem_read(r, &unique_id_html_color, sizeof(unique_id_html_color));
    sx_mem_read(r, &us_start, sizeof(us_start));
    sx_mem_read(r, &us_length, sizeof(us_length));
    sx_mem_read(r, &us_self, sizeof(us_self));
    sx_mem_read(r, &num_calls, sizeof(num_calls));
    sx_mem_read(r, &max_recurse_depth, sizeof(max_recurse_depth));

    float length_ms = (float)us_length / 1000.0f;
    float self_ms = (float)us_self / 1000.0f;

    char msg[256];
    sx_snprintf(msg, sizeof(msg), "\tname: 0x%x, Time: %.3f, Self: %.3f\n", name_hash, length_ms, self_ms);
    //OutputDebugStringA(msg);

    uint32_t num_childs;
    sx_mem_read(r, &num_childs, sizeof(num_childs));
    for (uint32_t i = 0; i < num_childs; i++) {
        rizz__rmt_read_sample(r);
    }
}

// TODO: override this for remotery and capture the frames
static void rizz__rmt_view_handler(const void* data, uint32_t size, void* context)
{
    sx_unused(context);

    sx_mem_reader r;
    sx_mem_init_reader(&r, data, size);

    const uint32_t smpl_fourcc = sx_makefourcc('S', 'M', 'P', 'L');

    #define WEBSOCKET_MAX_FRAME_HEADER_SIZE 10
    char empty_frame_header[WEBSOCKET_MAX_FRAME_HEADER_SIZE];
    sx_mem_read(&r, empty_frame_header, sizeof(empty_frame_header));

    uint32_t buff_size = *((uint32_t*)(empty_frame_header + 4));
    sx_unused(buff_size);

    uint32_t flag[2];
    sx_mem_read(&r, flag, sizeof(flag));
    if (flag[0] != smpl_fourcc) {
        return;
    }

    char thread_name[256];
    uint32_t num_samples;
    uint32_t digest_hash;
    rizz__rmt_read_string(&r, thread_name, sizeof(thread_name));
    sx_mem_read(&r, &num_samples, sizeof(num_samples));
    sx_mem_read(&r, &digest_hash, sizeof(digest_hash));

    //char msg[256];
    //sx_snprintf(msg, sizeof(msg), "%s:\n", thread_name);
    // OutputDebugStringA(msg);

    // read sample tree
    rizz__rmt_read_sample(&r);

    return;
}

static void rizz__console_shortcut_callback(void* user)
{
    rizz__core_cmd* c = user;
    the__core.execute_console_command(c->name);
}

static void rizz__register_console_command(const char* cmd, rizz_core_cmd_cb* callback, 
                                           const char* shortcut, void* user)
{
    sx_assert(cmd);
    sx_assert(cmd[0]);
    sx_assert(callback);

    rizz__core_cmd c;
    sx_strcpy(c.name, sizeof(c.name), cmd);
    c.callback = callback;
    c.user = user;
    sx_array_push(rizz__alloc(RIZZ_MEMID_CORE), g_core.console_cmds, c);

    if (shortcut && shortcut[0]) {
        the__app.register_shortcut(shortcut, rizz__console_shortcut_callback, &sx_array_last(g_core.console_cmds));
    }
}

static void rizz__execute_console_command(const char* cmd_and_args)
{
    // parse text into argc/argv
    int argc = 0;
    char arg[256];

    const char* cline = sx_skip_whitespace(cmd_and_args);
    while (cline && *cline) {
        for (char c = *cline; !sx_isspace(c) && *cline; c = *(++cline)) {
            if (*cline == '\"') {
                sx_str_block block = sx_findblock(cline, '\"', '\"');
                if (block.end) {
                    cline = block.end + 1;
                }
            }
        }
        cline = sx_skip_whitespace(cline);
        argc++;
    }
    if (argc == 0)
        return;

    const sx_alloc* alloc = the__core.alloc(RIZZ_MEMID_CORE);
    char** argv = sx_malloc(alloc, sizeof(char*) * argc);
    sx_assert(argv);

    cline = sx_skip_whitespace(cmd_and_args);
    int arg_idx = 0;
    while (cline && *cline) {
        int char_idx = 0;
        for (char c = *cline; !sx_isspace(c) && *cline && char_idx < (int)sizeof(arg); c = *(++cline), ++char_idx) {
            if (*cline == '\"') {
                sx_str_block block = sx_findblock(cline, '\"', '\"');
                if (block.end) {
                    cline = block.end + 1;
                    char_idx = (int)(uintptr_t)(block.end - block.start + 1);
                    sx_memcpy(arg, block.start, char_idx);
                    break;
                }
            } else {
                arg[char_idx] = c;
            }
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
            if ((r = g_core.console_cmds[i].callback(argc, argv, g_core.console_cmds[i].user)) <
                0) {
                rizz__log_warn("command '%s' failed with error code %d", argv[0], r);
            }
            break;
        }
    }

    if (!cmd_found) {
        rizz__log_warn("command '%s' not found", argv[0]);
    }

    for (int i = 0; i < argc; i++) {
        sx_free(alloc, argv[i]);
    }
    sx_free(alloc, argv);

}

static void* rizz__tmp_alloc_debug_cb(void* ptr, size_t size, uint32_t align, const char* file,
                                      const char* func, uint32_t line, void* user_data)
{
    rizz__core_tmpalloc_debug_inst* inst = user_data;

    // no free function
    if (!size) {
        return NULL;
    }

    align = align < SX_CONFIG_ALLOCATOR_NATURAL_ALIGNMENT ? SX_CONFIG_ALLOCATOR_NATURAL_ALIGNMENT : align;
    size_t aligned_size = sx_align_mask(size, align - 1);
    rizz__core_tmpalloc_debug* owner = inst->owner;

    if ((owner->offset + aligned_size) > owner->max_size) {
        sx_out_of_memory();
        return NULL;
    }

    if (ptr) {
        ptr = sx__realloc(g_core.heap_alloc, ptr, size, align, file, func, line);
    } else {
        ptr = sx__malloc(g_core.heap_alloc, size, align, file, func, line);
    }
    
    if (ptr) {
        rizz__core_tmpalloc_debug_item item = {
            .ptr = ptr,
            .line = line,
            .size = aligned_size
        };
        if (file) {
            sx_os_path_basename(item.file, sizeof(item.file), file);
        }
        owner->offset += aligned_size;
        owner->frame_peak = sx_max(owner->frame_peak, owner->offset);
        owner->peak = sx_max(owner->peak, owner->offset);
        sx_array_push(the__core.alloc(RIZZ_MEMID_CORE), owner->items, item);
    }

    return ptr;
}

static void* rizz__tmp_alloc_cb(void* ptr, size_t size, uint32_t align, const char* file,
                                const char* func, uint32_t line, void* user_data)
{
    sx_unused(file);
    sx_unused(line);
    sx_unused(func);

    rizz__core_tmpalloc_inst* inst = user_data;

    // we have no free function
    if (!size) {
        return NULL;
    }

    align = align < SX_CONFIG_ALLOCATOR_NATURAL_ALIGNMENT ? SX_CONFIG_ALLOCATOR_NATURAL_ALIGNMENT : align;
    size_t aligned_size = sx_align_mask(size, align - 1);

    // decide with side to allocate:
    //  - correct stack-mode allocations start from the end of the buffer
    //  - in case there is collapsing allocators (tmpalloc from lower stack level needs to allocate)
    //    then start from the begining of the buffer
    bool alloc_from_start = (inst->depth < inst->parent->stack_depth);
    uint8_t* buff = (uint8_t*)inst->parent->vmem.ptr;
    size_t buff_size = (size_t)inst->parent->vmem.page_size * (size_t)inst->parent->vmem.max_pages;
    
    if (!alloc_from_start) {
        size_t end_offset = inst->end_offset + aligned_size;
        if (end_offset % align != 0) {
            sx_align_mask(end_offset, align - 1);
        }

        void* new_ptr = buff + buff_size - end_offset;
        *((size_t*)new_ptr - 1) = size;
        inst->end_offset = end_offset + sizeof(size_t);

        if (inst->end_offset > buff_size || (buff_size - inst->end_offset) < inst->start_offset) {
            sx_out_of_memory();
            return NULL;
        }

        if (ptr) {
            size_t old_size = *((size_t*)ptr - 1);
            sx_memmove(new_ptr, ptr, sx_min(old_size, size));
        } 

        size_t total = inst->start_offset + inst->end_offset;
        inst->parent->peak = sx_max(inst->parent->peak, total);
        inst->parent->frame_peak = sx_max(inst->parent->frame_peak, total);

        return new_ptr;
    } else {
        void* lastptr = buff + inst->start_lastptr_offset;
        // get the end_offset from the current depth allocator instead of self
        size_t end_offset = inst->parent->alloc_stack[inst->parent->stack_depth-1].end_offset;

        // if we are realloc on the same pointer, just re-adjust the offset
        if (ptr == lastptr) {
            size_t lastsize = *((size_t*)ptr - 1);
            lastsize = sx_align_mask(lastsize, align - 1);
            inst->start_offset += (aligned_size > lastsize) ? (aligned_size - lastsize) : 0;
            if (inst->start_offset > (buff_size - end_offset)) {
                sx_out_of_memory();
                return NULL;
            }
            *((size_t*)ptr - 1) = size;

            size_t total = inst->start_offset + end_offset;
            inst->parent->peak = sx_max(inst->parent->peak, total);
            inst->parent->frame_peak = sx_max(inst->parent->frame_peak, total);
            return ptr;
        } else {
            size_t start_offset = inst->start_offset + sizeof(size_t);
            if (start_offset % align != 0) {
                sx_align_mask(start_offset, align - 1);
            }

            if ((start_offset + aligned_size) > (buff_size - end_offset)) {
                sx_out_of_memory();
                return NULL;
            }

            void* new_ptr = buff + start_offset;
            *((size_t*)new_ptr - 1) = size;
            inst->start_offset = start_offset + aligned_size;
            inst->start_lastptr_offset = start_offset;

            size_t total = inst->start_offset + end_offset;
            inst->parent->peak = sx_max(inst->parent->peak, total);
            inst->parent->frame_peak = sx_max(inst->parent->frame_peak, total);
            return new_ptr;
        }
    }
}

static int rizz__core_echo_command(int argc, char* argv[], void* user)
{
    sx_unused(user);
    
    if (argc > 1) {
        rizz__log_debug(argv[1]);
        return 0;
    }

    return -1;
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

    #if RIZZ_CONFIG_DEBUG_MEMORY
        rizz__mem_init(RIZZ_MEMOPTION_TRACE_CALLSTACK|RIZZ_MEMOPTION_MULTITHREAD);
        // initialize track allocators
        for (int i = 0; i < _RIZZ_MEMID_COUNT; i++) {
            g_core.track_allocs[i] = rizz__mem_create_allocator(
                k__memid_names[i], 
                RIZZ_MEMOPTION_INHERIT,
                NULL,
                g_core.heap_alloc);
            if (!g_core.track_allocs[i]) {
                rizz__log_error("creating track allocators failed");
                return false;
            }
        }
    #else
        g_core.heap_proxy_alloc = *g_core.heap_alloc;
    #endif

    const sx_alloc* alloc = rizz__alloc(RIZZ_MEMID_CORE);
    sx_strcpy(g_core.app_name, sizeof(g_core.app_name), conf->app_name);
    g_core.app_ver = conf->app_version;
    g_core.flags = conf->core_flags;
    g_core.log_level = conf->log_level;

    // string pool, used mainly for allocating permanent const char* pointers
    g_core.strpool = sx_strpool_create(alloc, NULL);
    sx_assert_alwaysf(g_core.strpool, "out of memory");

    int num_worker_threads = conf->job_num_threads >= 0 ? conf->job_num_threads : (sx_os_numcores() - 1);
    num_worker_threads = sx_max(1, num_worker_threads);   // we should have at least one worker thread
    g_core.num_threads = num_worker_threads + 1;    // include the main-thread

    // log queues per thread
    g_core.log_pipes = sx_malloc(alloc, sizeof(rizz__log_pipe) * g_core.num_threads);
    if (!g_core.log_pipes) {
        sx_out_of_memory();
        return false;
    }
    for (int i = 0; i < g_core.num_threads; i++) {
        g_core.log_pipes[i].queue = sx_queue_spsc_create(alloc, sizeof(rizz__log_entry_internal), 32);
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

    // disk-io (virtual file system)
    if (!rizz__vfs_init(rizz__alloc(RIZZ_MEMID_VFS))) {
        rizz__log_error("initializing disk-io failed");
        return false;
    }
    rizz__log_info("(init) vfs");

    { // Temp allocators
        size_t page_sz = sx_os_pagesz();
        size_t tmp_size = sx_align_mask(conf->tmp_mem_max > 0 ? conf->tmp_mem_max * 1024 : DEFAULT_TMP_SIZE, page_sz - 1);

        if (!(g_core.flags & RIZZ_CORE_FLAG_DEBUG_TEMP_ALLOCATOR)) {
            g_core.tmp_allocs = sx_calloc(alloc, sizeof(rizz__core_tmpalloc) * g_core.num_threads);
            if (!g_core.tmp_allocs) {
                sx_out_of_memory();
                return false;
            }

            int num_tmp_pages =  sx_vmem_get_needed_pages(tmp_size);

            for (int i = 0; i < g_core.num_threads; i++) {
                rizz__core_tmpalloc* t = &g_core.tmp_allocs[i];
                if (!sx_vmem_init(&t->vmem, 0, num_tmp_pages)) {
                    sx_out_of_memory();
                    return false;
                }
                sx_vmem_commit_pages(&t->vmem, 0, num_tmp_pages);
            }
        }
        else {
            g_core.tmp_debug_allocs = sx_calloc(alloc, sizeof(rizz__core_tmpalloc_debug)*g_core.num_threads);
            if (!g_core.tmp_debug_allocs) {
                sx_out_of_memory();
                return false;
            }

            for (int i = 0; i < g_core.num_threads; i++) {
                rizz__core_tmpalloc_debug* t = &g_core.tmp_debug_allocs[i];
                t->max_size = tmp_size;
            }

            rizz__log_info("(init) using debug temp allocators");
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
    #if RMT_ENABLED 
        sx_mutex_init(&g_core.rmt_mtx);
        g_core.rmt_alloc_pool = sx_pool_create(rizz__alloc(RIZZ_MEMID_TOOLSET), RMT_SMALL_MEMORY_SIZE, 1000);
        if (g_core.rmt_alloc_pool == NULL) {
            sx_memory_fail();
            return false;
        }
        rmtSettings* rmt_config = rmt_Settings();
        if (rmt_config) {
            // FIXME: removed due to various regression bugs
            //        at some point, we need to get back to the newer versions and recheck this
            // rmt_config->enableThreadSampler = false; // FIXME: Due to assert failure we had to disable this feature: 
                                                        // https://github.com/Celtoys/Remotery/issues/178#issue-894595107
            rmt_config->malloc = rmt__malloc;
            rmt_config->free = rmt__free;
            rmt_config->realloc = rmt__realloc;
            rmt_config->mm_context = (void*)rizz__alloc(RIZZ_MEMID_TOOLSET);
            rmt_config->port = (uint16_t)conf->profiler_listen_port;
            rmt_config->msSleepBetweenServerUpdates = conf->profiler_update_interval_ms;
            rmt_config->reuse_open_port = true;
            rmt_config->input_handler = rizz__rmt_input_handler;
            rmt_config->input_handler_context = (void*)rizz__alloc(RIZZ_MEMID_TOOLSET);
            rmt_config->view_handler = rizz__rmt_view_handler;
        }
        rmtError rmt_err;
        if ((rmt_err = rmt_CreateGlobalInstance(&g_core.rmt)) != RMT_ERROR_NONE) {
            rizz__log_warn("initializing profiler failed: %d", rmt_err);
        }

        rmt_SetCurrentThreadName("Main");
        if (g_core.rmt) {
            const char* profile_subsets;
            if (conf->core_flags & RIZZ_CORE_FLAG_PROFILE_GPU)
                profile_subsets = "cpu/gpu";
            else
                profile_subsets = "cpu";
            rizz__log_info("(init) profiler (%s): port=%d", profile_subsets, conf->profiler_listen_port);

            g_core.rmt_command_queue = sx_queue_spsc_create(alloc, sizeof(char*), 16);
            sx_assert_always(g_core.rmt_command_queue);
        }
    #endif // RMT_ENABLED

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
        sx_coro_create_context(alloc, conf->coro_num_init_fibers, conf->coro_stack_size * 1024);
    if (!g_core.coro) {
        rizz__log_error("initializing coroutines failed");
        return false;
    }
    rizz__log_info("(init) coroutines: stack_size=%dkb", conf->coro_stack_size);

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

    rizz__json_init();

    the__core.register_console_command("echo", rizz__core_echo_command, NULL, NULL);

    return true;
}

#ifdef _DEBUG
static void rizz__core_dump_leak(const char* formatted_msg, 
                                 const char* file, const char* func,
                                 int line, size_t size, void* ptr)
{
    sx_unused(formatted_msg);
    the__core.print_debug(0, file, line, "MEMORY LEAK: @%s, %$ubytes (ptr=0x%p)", func, size, ptr);
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
        sx_coro_destroy_context(g_core.coro);
    }

    if (g_core.flags & RIZZ_CORE_FLAG_DUMP_UNUSED_ASSETS) {
        rizz__asset_dump_unused("unused-assets.txt");
    }

    // Release native subsystems
    rizz__http_release();
    rizz__asset_release();
    rizz__gfx_release();
    rizz__vfs_release();

    #if RMT_ENABLED
        sx_queue_spsc_destroy(g_core.rmt_command_queue, alloc);
        if (g_core.rmt) {
            rmt_DestroyGlobalInstance(g_core.rmt);
        }
        sx_pool_destroy(g_core.rmt_alloc_pool, rizz__alloc(RIZZ_MEMID_TOOLSET));
        sx_mutex_exit(&g_core.rmt_mtx);
    #endif // RMT_ENABLED
    sx_array_free(alloc, g_core.console_cmds);

    if (g_core.tmp_allocs) {
        for (int i = 0; i < g_core.num_threads; i++) {
            sx_vmem_release(&g_core.tmp_allocs[i].vmem);
            sx_array_free(alloc, g_core.tmp_allocs[i].alloc_stack);
        }
        sx_free(alloc, g_core.tmp_allocs);
    }

    if (g_core.tmp_debug_allocs) {
        for (int i = 0; i < g_core.num_threads; i++) {
            sx_array_free(alloc, g_core.tmp_debug_allocs[i].items);
            sx_array_free(alloc, g_core.tmp_debug_allocs[i].alloc_stack);
        }
        sx_free(alloc, g_core.tmp_debug_allocs);
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

    sx_strpool_destroy(g_core.strpool, alloc);

    #if RIZZ_CONFIG_DEBUG_MEMORY
        for (int i = 0; i < _RIZZ_MEMID_COUNT; i++) {
            rizz__mem_destroy_allocator(g_core.track_allocs[i]);
        }
        rizz__mem_release();
    #endif
    
    for (int i = 0; i < sx_array_count(g_core.tls_vars); i++) {
        if (g_core.tls_vars[i].tls) {
            sx_tls_destroy(g_core.tls_vars[i].tls);
        }
    }

    sx_array_free(g_core.heap_alloc, g_core.tls_vars);
    rizz__log_info("shutdown");

#ifdef _DEBUG
    sx_dump_leaks(rizz__core_dump_leak);
#endif
    sx_memset(&g_core, 0x0, sizeof(g_core));
}

void rizz__core_frame()
{
    if (g_core.paused) {
        return;
    }

    rizz__profile(Frame) {
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
        if (g_core.tmp_allocs) {
            for (int i = 0, c = g_core.num_threads; i < c; i++) {
                rizz__core_tmpalloc* t = &g_core.tmp_allocs[i];
            
                if (t->stack_depth > 0) {
                    t->wait_time += dt;
                    if (t->wait_time > MAX_TEMP_ALLOC_WAIT_TIME) {
                        the__core.print_error(0,
                                              t->alloc_stack[t->stack_depth-1].file, 
                                              t->alloc_stack[t->stack_depth-1].line,
                                              "tmp_alloc_push doesn't seem to have the pop call (Thread: %d)", i);
                        sx_assertf(0, "not all tmp_allocs are popped.");
                    }
                } else {
                    sx_array_clear(t->alloc_stack);
                    t->stack_depth = 0;
                    t->frame_peak = 0;
                    t->wait_time = 0;
                }
            }
        } else if (g_core.tmp_debug_allocs) {
            for (int i = 0, c = g_core.num_threads; i < c; i++) {
                rizz__core_tmpalloc_debug* t = &g_core.tmp_debug_allocs[i];
                if (t->stack_depth > 0) {
                    t->wait_time += dt;
                    if (t->wait_time > MAX_TEMP_ALLOC_WAIT_TIME) {
                        the__core.print_error(0, 
                                              t->alloc_stack[t->stack_depth - 1].file,
                                              t->alloc_stack[t->stack_depth - 1].line,
                                              "tmp_alloc_push doesn't seem to have the pop call");
                        sx_assertf(0, "not all tmp_allocs are popped.");
                    }
                } else {
                    sx_assertf(sx_array_count(t->items) == 0, "not all tmp_alloc items are freed");
                    sx_array_clear(t->items);
                    sx_array_clear(t->alloc_stack);
                    t->offset = 0;
                    t->stack_depth = 0;
                    t->frame_peak = 0;
                    t->wait_time = 0;
                }
            }
        }

        rizz__gfx_trace_reset_frame_stats(RIZZ_GFX_TRACE_COMMON);

        // update internal sub-systems
        rizz__http_update();
        rizz__vfs_async_update();
        rizz__asset_update();
        rizz__gfx_update();

        rizz__profile(Coroutines) {
            sx_coro_update(g_core.coro, dt);
        }

        // update plugins and application
        rizz__plugin_update(dt);

        // execute remaining commands from the 'staged' API
        rizz__profile(Execute_command_buffers) {
            rizz__gfx_execute_command_buffers_final();
        }

        // flush queued logs
        rizz__profile(Log_update) {
            rizz__log_update();
        }

        // consume console commands from remotery
        if (g_core.rmt_command_queue) {
            char* cmd;
            while (sx_queue_spsc_consume(g_core.rmt_command_queue, (void*)&cmd)) {
                rizz__execute_console_command(cmd);
                sx_free(the__core.alloc(RIZZ_MEMID_TOOLSET), cmd);
            }
        }

        // draw imgui stuff
        rizz_api_imgui* the_imgui = the__plugin.get_api_byname("imgui", 0);
        if (the_imgui) {
            rizz__profile(ImGui_draw) {
                rizz_api_imgui_extra* the_imguix = the__plugin.get_api_byname("imgui_extra", 0);
                if (g_core.show_memory.show) {
                    rizz__mem_show_debugger(g_core.show_memory.p_open);
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
            }
        }

        rizz__gfx_commit_gpu();
        ++g_core.frame_idx;

        the__gfx.imm.end_profile_sample();
    } // profile
}

static const sx_alloc* rizz__core_tmp_alloc_push_trace(const char* file, uint32_t line)
{
    if (g_core.tmp_allocs) {
        rizz__core_tmpalloc* talloc = &g_core.tmp_allocs[sx_job_thread_index(g_core.jobs)];

        int count = sx_array_count(talloc->alloc_stack);
        if (count == 0) {
            rizz__core_tmpalloc_inst inst = {
                .alloc = {
                    .alloc_cb = rizz__tmp_alloc_cb,
                },
                .depth = count + 1,
                .parent = talloc,
                .file = file,
                .line = line
            };

            sx_array_push(rizz__alloc(RIZZ_MEMID_CORE), talloc->alloc_stack, inst);
        } else {
            rizz__core_tmpalloc_inst inst = sx_array_last(talloc->alloc_stack);
            ++inst.depth;
            inst.file = file;
            inst.line = line;
            sx_array_push(rizz__alloc(RIZZ_MEMID_CORE), talloc->alloc_stack, inst);
        }

        sx_atomic_incr(&talloc->stack_depth);
        rizz__core_tmpalloc_inst* _inst = &talloc->alloc_stack[count];
        _inst->alloc.user_data = _inst;
        return &_inst->alloc;
    } 
    else {
        sx_assert(g_core.tmp_debug_allocs);
        rizz__core_tmpalloc_debug* talloc = &g_core.tmp_debug_allocs[sx_job_thread_index(g_core.jobs)];

        int count = sx_array_count(talloc->alloc_stack);
        int item_idx = sx_array_count(talloc->items);
        if (count == 0) {
            rizz__core_tmpalloc_debug_inst inst = {
                .alloc = {
                    .alloc_cb = rizz__tmp_alloc_debug_cb
                },
                .owner = talloc,
                .item_idx = item_idx,
                .file = file,
                .line = line
            };
            sx_array_push(rizz__alloc(RIZZ_MEMID_CORE), talloc->alloc_stack, inst);
        } else {
            rizz__core_tmpalloc_debug_inst inst = sx_array_last(talloc->alloc_stack);
            inst.item_idx = item_idx;
            inst.file = file;
            inst.line = line;
            sx_array_push(rizz__alloc(RIZZ_MEMID_CORE), talloc->alloc_stack, inst);
        }

        sx_atomic_incr(&talloc->stack_depth);
        rizz__core_tmpalloc_debug_inst* _inst = &talloc->alloc_stack[count];
        _inst->alloc.user_data = _inst;
        return &_inst->alloc;
    }
}

static const sx_alloc* rizz__core_tmp_alloc_push(void)
{
    return rizz__core_tmp_alloc_push_trace(NULL, 0);
}

static void rizz__core_tmp_alloc_pop(void)
{
    if (g_core.tmp_allocs) {
        rizz__core_tmpalloc* talloc = &g_core.tmp_allocs[sx_job_thread_index(g_core.jobs)];
        if (sx_array_count(talloc->alloc_stack)) {
            sx_array_pop_last(talloc->alloc_stack);
            sx_assert(talloc->stack_depth > 0);
            sx_atomic_decr(&talloc->stack_depth);
        } else {
            sx_assertf(0, "no matching tmp_alloc_push for the call tmp_alloc_pop");
        }
    }
    else {
        sx_assert(g_core.tmp_debug_allocs);
        rizz__core_tmpalloc_debug* talloc = &g_core.tmp_debug_allocs[sx_job_thread_index(g_core.jobs)];
        if (sx_array_count(talloc->alloc_stack)) {
            rizz__core_tmpalloc_debug_inst inst = sx_array_last(talloc->alloc_stack);
            // free all allocated items from item_idx to the last one
            int count = sx_array_count(talloc->items);
            for (int i = inst.item_idx; i < count; i++) {
                sx_free(g_core.heap_alloc, talloc->items[i].ptr);
                talloc->offset -= talloc->items[i].size;
            }

            if (talloc->items) {
                sx_array_pop_lastn(talloc->items, (count - inst.item_idx));
            }

            sx_array_pop_last(talloc->alloc_stack);
            sx_assert(talloc->stack_depth > 0);
            sx_atomic_decr(&talloc->stack_depth);
        } else {
            sx_assertf(0, "no matching tmp_alloc_push for the call tmp_alloc_pop");
        }
    }
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
                                    void* (*init_cb)(int thread_idx, uint32_t thread_id, void* user))
{
    sx_assert(name);
    sx_assert(init_cb);

    rizz__tls_var tvar = (rizz__tls_var){ .name_hash = sx_hash_fnv32_str(name),
                                          .user = user,
                                          .init_cb = init_cb,
                                          .tls = sx_tls_create() };

    sx_assert(tvar.tls);

    sx_array_push(g_core.heap_alloc, g_core.tls_vars, tvar);
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

    sx_assertf(0, "tls_var not registered");
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

static void rizz__pause(void)
{
    g_core.paused = true;
}

static void rizz__resume(void)
{
    g_core.last_tick = sx_tm_now();
    g_core.paused = false;
}

static bool rizz__is_paused(void)
{
    return g_core.paused;
}

static const char* rizz__str_alloc(sx_str_t* phandle, const char* fmt, ...)
{
    sx_str_t handle;
    rizz__with_temp_alloc(tmp_alloc) {
        va_list args;
        va_start(args, fmt);
        char* str = sx_vsnprintf_alloc(tmp_alloc, fmt, args);
        va_end(args);

        handle = sx_strpool_add(g_core.strpool, str, sx_strlen(str));

        sx_assert(handle);
        if (phandle) {
            *phandle = handle;
        }
    }

    return sx_strpool_cstr(g_core.strpool, handle);
}

static void rizz__str_free(sx_str_t handle)
{
    sx_strpool_del(g_core.strpool, handle);
}

static const char* rizz__str_cstr(sx_str_t handle)
{
    sx_assert(handle);
    return sx_strpool_cstr(g_core.strpool, handle);
}

// Core API
rizz_api_core the__core = { .heap_alloc = rizz__heap_alloc,
                            .tmp_alloc_push = rizz__core_tmp_alloc_push,
                            .tmp_alloc_pop = rizz__core_tmp_alloc_pop,
                            .tmp_alloc_push_trace = rizz__core_tmp_alloc_push_trace,
                            .tls_register = rizz__core_tls_register,
                            .tls_var = rizz__core_tls_var,
                            .alloc = rizz__alloc,
                            .trace_alloc_destroy = rizz__mem_destroy_allocator,
                            .trace_alloc_clear = rizz__mem_allocator_clear_trace,
                            .version = rizz__version,
                            .delta_tick = rizz__delta_tick,
                            .delta_time = rizz__delta_time,
                            .elapsed_tick = rizz__elapsed_tick,
                            .fps = rizz__fps,
                            .fps_mean = rizz__fps_mean,
                            .frame_index = rizz__frame_index,
                            .pause = rizz__pause,
                            .resume = rizz__resume,
                            .is_paused = rizz__is_paused,
                            .set_cache_dir = rizz__set_cache_dir,
                            .cache_dir = rizz__cache_dir,
                            .data_dir = rizz__data_dir,
                            .str_alloc = rizz__str_alloc,
                            .str_free = rizz__str_free,
                            .str_cstr = rizz__str_cstr,
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
                            .register_console_command = rizz__register_console_command,
                            .execute_console_command = rizz__execute_console_command,
                            .show_graphics_debugger = rizz__show_graphics_debugger,
                            .show_memory_debugger = rizz__show_memory_debugger,
                            .show_log = rizz__show_log };
