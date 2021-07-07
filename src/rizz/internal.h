#pragma once

#ifndef RIZZ_INTERNAL_API
#    error "this file should only included from rizz source"
#endif

#include "rizz/rizz.h"

RIZZ_API rizz_api_core the__core;
RIZZ_API rizz_api_plugin the__plugin;
RIZZ_API rizz_api_vfs the__vfs;
RIZZ_API rizz_api_asset the__asset;
RIZZ_API rizz_api_gfx the__gfx;
RIZZ_API rizz_api_refl the__refl;
RIZZ_API rizz_api_http the__http;
RIZZ_API rizz_api_app the__app;
RIZZ_API rizz_api_camera the__camera;

#ifdef __cplusplus
extern "C" {
#endif

bool rizz__plugin_init(const sx_alloc* alloc, const char* plugin_path);
void rizz__plugin_release();
void rizz__plugin_broadcast_event(const rizz_app_event* e);
void rizz__plugin_update(float dt);
bool rizz__plugin_load_abs(const char* filepath, bool entry, const char** deps, int num_deps);
bool rizz__plugin_init_plugins();

bool rizz__core_init(const rizz_config* conf);
void rizz__core_release();
void rizz__core_frame();
void rizz__core_fix_callback_ptrs(const void** ptrs, const void** new_ptrs, int num_ptrs);

typedef struct mem_trace_context mem_trace_context;
bool rizz__mem_init(uint32_t opts);
void rizz__mem_release(void);
sx_alloc* rizz__mem_create_allocator(const char* name, uint32_t mem_opts, const char* parent, const sx_alloc* alloc);
void rizz__mem_destroy_allocator(sx_alloc* alloc);
void rizz__mem_allocator_clear_trace(sx_alloc* alloc);
void rizz__mem_show_debugger(bool*);
void rizz__mem_reload_modules(void);

// windows.h
bool rizz__win_get_vstudio_dir(char* vspath, size_t vspath_size);

// logging
#define rizz__log_info(_text, ...)     the__core.print_info(0, __FILE__, __LINE__, _text, ##__VA_ARGS__)
#define rizz__log_debug(_text, ...)    the__core.print_debug(0, __FILE__, __LINE__, _text, ##__VA_ARGS__)
#define rizz__log_verbose(_text, ...)  the__core.print_verbose(0, __FILE__, __LINE__, _text, ##__VA_ARGS__)
#define rizz__log_error(_text, ...)    the__core.print_error(0, __FILE__, __LINE__, _text, ##__VA_ARGS__)
#define rizz__log_warn(_text, ...)     the__core.print_warning(0, __FILE__, __LINE__, _text, ##__VA_ARGS__)

// coroutines
#define rizz__coro_declare(_name)          static void coro__##_name(sx_fiber_transfer __transfer)
#define rizz__coro_userdata()              __transfer.user
#define rizz__coro_end()                   the__core.coro_end(&__transfer.from)
#define rizz__coro_wait(_msecs)            the__core.coro_wait(&__transfer.from, (_msecs))
#define rizz__coro_yield()                 the__core.coro_yield(&__transfer.from, 1)
#define rizz__coro_yieldn(_n)              the__core.coro_yield(&__transfer.from, (_n))
#define rizz__coro_invoke(_name, _user)    the__core.coro_invoke(coro__##_name, (_user))

#define rizz__with_temp_alloc(_name) sx_with(const sx_alloc* _name = the__core.tmp_alloc_push(), \
                                             the__core.tmp_alloc_pop()) 

#define rizz__profile_begin(_name, _flags) \
        static uint32_t rmt_sample_hash_##_name = 0; \
        uint32_t rmt_sample_raii_##_name; \
        the__core.begin_profile_sample(#_name, _flags, &rmt_sample_hash_##_name)
#define rizz__profile_end(_name)               \
        (void)rmt_sample_raii_##_name;   \
        the__core.end_profile_sample();

// usage pattern:
// rizz__profile(name) {
//  ...
// } // automatically ends profile sample
#define rizz__profile(_name) static uint32_t sx_concat(rmt_sample_hash_, _name) = 0; \
        sx_defer(the__core.begin_profile_sample(sx_stringize(_name), 0, &sx_concat(rmt_sample_hash_, _name)), \
                 the__core.end_profile_sample())

bool rizz__vfs_init(const sx_alloc* alloc);
void rizz__vfs_release();
void rizz__vfs_async_update();

bool rizz__asset_init(const sx_alloc* alloc, const char* dbfile, const char* variation);
bool rizz__asset_dump_unused(const char* filepath);
void rizz__asset_release();
void rizz__asset_update();

bool rizz__gfx_init(const sx_alloc* alloc, const sg_desc* desc, bool enable_profile);
void rizz__gfx_release();
void rizz__gfx_trace_reset_frame_stats(rizz_gfx_perframe_trace_zone zone);
void rizz__gfx_execute_command_buffers_final();
void rizz__gfx_update();
void rizz__gfx_commit_gpu();

bool rizz__http_init(const sx_alloc* alloc);
void rizz__http_release();
void rizz__http_update();

typedef struct sg_desc sg_desc;
void rizz__app_init_gfx_desc(sg_desc* desc);
const void* rizz__app_d3d11_device(void);
const void* rizz__app_d3d11_device_context(void);

void rizz__json_init(void);

#ifdef __cplusplus
}
#endif