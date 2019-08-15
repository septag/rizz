//
// Copyright 2019 Sepehr Taghdisian (septag@github). All rights reserved.
// License: https://github.com/septag/rizz#license-bsd-2-clause
//
#pragma once

#include "types.h"

#include "sx/fiber.h"
#include "sx/jobs.h"

#define RIZZ_MAX_TEMP_ALLOCS 32

typedef struct rizz_config rizz_config;    // #include "rizz/game.h"

// custom console commands that can be registered (sent via profiler)
// return >= 0 for success and -1 for failure
typedef int(rizz_core_cmd_cb)(int argc, char* argv[]);

typedef enum rizz_mem_id {
    RIZZ_MEMID_CORE = 0,
    RIZZ_MEMID_GRAPHICS,
    RIZZ_MEMID_AUDIO,
    RIZZ_MEMID_VFS,
    RIZZ_MEMID_REFLECT,
    RIZZ_MEMID_OTHER,
    RIZZ_MEMID_DEBUG,
    RIZZ_MEMID_TOOLSET,
    RIZZ_MEMID_INPUT,
    RIZZ_MEMID_GAME,
    _RIZZ_MEMID_COUNT
} rizz_mem_id;

typedef struct rizz_track_alloc_item {
    char file[32];
    char func[64];
    void* ptr;
    int64_t size;
    int line;
} rizz_track_alloc_item;

typedef struct {
    int offset;
    int size;
    int peak;
} rizz_linalloc_info;

typedef struct {
    const char* name;
    const rizz_track_alloc_item* items;
    int num_items;
    int mem_id;
    int64_t size;
    int64_t peak;
} rizz_trackalloc_info;

typedef struct rizz_mem_info {
    rizz_trackalloc_info trackers[_RIZZ_MEMID_COUNT];
    rizz_linalloc_info temp_allocs[RIZZ_MAX_TEMP_ALLOCS];
    int num_trackers;
    int num_temp_allocs;
    size_t heap;
    size_t heap_max;
    int heap_count;
} rizz_mem_info;

typedef struct rizz_api_core {
    // heap allocator: thread-safe, allocates dynamically from heap (libc->malloc)
    const sx_alloc* (*heap_alloc)();

    // temp stack allocator: fast and thread-safe (per job thread only).
    //                       Temp allocators behave like stack, so they can push() and pop()
    // NOTE: do not keep tmp_alloc memory between multiple frames,
    //       At the end of each frame, the tmp_allocs are reset
    const sx_alloc* (*tmp_alloc_push)();
    void (*tmp_alloc_pop)();

    // TLS functions are used for setting TLS variables to worker threads by an external source
    // register: use name to identify the variable (Id). (not thread-safe)
    // tls_var: gets pointer to variable, (thread-safe)
    //          `init_cb` will be called on the thread if it's the first time the variable is
    //          is fetched. you should initialize the variable and return it's pointer
    //          destroying tls variables are up to the user, after variable is destroyed, the return
    //          value of tls_var may be invalid
    void (*tls_register)(const char* name, void* user,
                         void* (*init_cb)(int thread_idx, uint32_t thread_id, void* user));
    void* (*tls_var)(const char* name);

    const sx_alloc* (*alloc)(rizz_mem_id id);

    void (*get_mem_info)(rizz_mem_info* info);

    // random
    uint32_t (*rand)();                       // 0..UNT32_MAX
    float (*randf)();                         // 0..1
    int (*rand_range)(int _min, int _max);    // _min.._max

    uint64_t (*delta_tick)();
    uint64_t (*elapsed_tick)();
    float (*fps)();
    float (*fps_mean)();
    int64_t (*frame_index)();

    void (*set_cache_dir)(const char* path);
    const char* (*cache_dir)();
    const char* (*data_dir)();

    // jobs
    sx_job_t (*job_dispatch)(int count,
                             void (*callback)(int start, int end, int thrd_index, void* user),
                             void* user, sx_job_priority priority, uint32_t tags);
    void (*job_wait_and_del)(sx_job_t job);
    bool (*job_test_and_del)(sx_job_t job);
    int (*job_num_workers)();

    void (*coro_invoke)(void (*coro_cb)(sx_fiber_transfer), void* user);
    void (*coro_end)(void* pfrom);
    void (*coro_wait)(void* pfrom, int msecs);
    void (*coro_yield)(void* pfrom, int nframes);

    void (*print_info)(const char* fmt, ...);
    void (*print_debug)(const char* fmt, ...);
    void (*print_verbose)(const char* fmt, ...);
    void (*print_error_trace)(const char* source_file, int line, const char* fmt, ...);
    void (*print_error)(const char* fmt, ...);
    void (*print_warning)(const char* fmt, ...);

    void (*begin_profile_sample)(const char* name, uint32_t flags, uint32_t* hash_cache);
    void (*end_profile_sample)();

    void (*register_console_command)(const char* cmd, rizz_core_cmd_cb* callback);
} rizz_api_core;

#ifdef RIZZ_INTERNAL_API
typedef struct rizz_gfx_cmdbuffer rizz_gfx_cmdbuffer;    // #include "rizz/graphics.h"

bool rizz__core_init(const rizz_config* conf);
void rizz__core_release();
void rizz__core_frame();
rizz_gfx_cmdbuffer* rizz__core_gfx_cmdbuffer();
RIZZ_API void rizz__core_fix_callback_ptrs(const void** ptrs, const void** new_ptrs, int num_ptrs);

RIZZ_API rizz_api_core the__core;

// clang-format off

// logging
#   define rizz_log_info(_text, ...)     the__core.print_info(_text, ##__VA_ARGS__)
#   define rizz_log_debug(_text, ...)    the__core.print_debug(_text, ##__VA_ARGS__)
#   define rizz_log_verbose(_text, ...)  the__core.print_verbose(_text, ##__VA_ARGS__)
#   define rizz_log_error(_text, ...)    the__core.print_error_trace(__FILE__, __LINE__, _text, ##__VA_ARGS__)
#   define rizz_log_warn(_text, ...)     the__core.print_warning(_text, ##__VA_ARGS__)

// coroutines
#   define rizz_coro_declare(_name)          static void coro__##_name(sx_fiber_transfer __transfer)
#   define rizz_coro_userdata()              __transfer.user
#   define rizz_coro_end()                   the__core.coro_end(&__transfer.from)
#   define rizz_coro_wait(_msecs)            the__core.coro_wait(&__transfer.from, (_msecs))
#   define rizz_coro_yield()                 the__core.coro_yield(&__transfer.from, 1)
#   define rizz_coro_yieldn(_n)              the__core.coro_yield(&__transfer.from, (_n))
#   define rizz_coro_invoke(_name, _user)    the__core.coro_invoke(coro__##_name, (_user))
#else
// logging
#   define rizz_log_info(_core, _text, ...)     _core->print_info(_text, ##__VA_ARGS__)
#   define rizz_log_debug(_core, _text, ...)    _core->print_debug(_text, ##__VA_ARGS__)
#   define rizz_log_verbose(_core, _text, ...)  _core->print_verbose(_text, ##__VA_ARGS__)
#   define rizz_log_error(_core, _text, ...)    _core->print_error_trace(__FILE__, __LINE__, _text, ##__VA_ARGS__)
#   define rizz_log_warn(_core, _text, ...)     _core->print_warning(_text, ##__VA_ARGS__)

// coroutines
#   ifdef __cplusplus
#       define rizz_coro_declare(_name)     \
            RIZZ_POINTER static auto coro__##_name = [](sx_fiber_transfer __transfer)
#   else
#       define rizz_coro_declare(_name)     \
            static void coro__##_name_(sx_fiber_transfer __transfer);   \
            RIZZ_POINTER static void (*coro__##_name)(sx_fiber_transfer __transfer) = coro__##_name_; \
            static void coro__##_name_(sx_fiber_transfer __transfer)
#   endif

#   define rizz_coro_userdata()                  __transfer.user
#   define rizz_coro_end(_core)                  _core->coro_end(&__transfer.from)
#   define rizz_coro_wait(_core, _msecs)         _core->coro_wait(&__transfer.from, (_msecs))
#   define rizz_coro_yield(_core)                _core->coro_yield(&__transfer.from, 1)
#   define rizz_coro_yieldn(_core, _n)           _core->coro_yield(&__transfer.from, (_n))
#   define rizz_coro_end(_core)                  _core->coro_end(&__transfer.from)
#   define rizz_coro_invoke(_core, _name, _user) _core->coro_invoke(coro__##_name, (_user))

// clang-format on

#    define rizz_profile_begin(_core, _name, _flags) \
        static uint32_t rmt_sample_hash_##_name = 0; \
        _core->begin_profile_sample(#_name, _flags, &rmt_sample_hash_##_name)
#    define rizz_profile_end(_core) _core->end_profile_sample()

#    ifdef __cplusplus
struct rizz_profile_scoped {
    rizz_api_core* _api;

    rizz_profile_scoped() = delete;
    rizz_profile_scoped(const rizz_profile_scoped&) = delete;
    rizz_profile_scoped(const rizz_profile_scoped&&) = delete;

    rizz_profile_scoped(rizz_api_core* api, const char* name, uint32_t flags, uint32_t* hash_cache)
    {
        _api = api;
        api->begin_profile_sample(name, flags, hash_cache);
    }

    ~rizz_profile_scoped() { _api->end_profile_sample(); }
};
#    endif

#endif
