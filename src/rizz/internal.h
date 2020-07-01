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

// clang-format off
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

#define rizz__profile_begin(_name, _flags) \
        static uint32_t rmt_sample_hash_##_name = 0; \
        uint32_t rmt_sample_raii_##_name; \
        the__core.begin_profile_sample(#_name, _flags, &rmt_sample_hash_##_name)
#define rizz__profile_end(_name)               \
        (void)rmt_sample_raii_##_name;   \
        the__core.end_profile_sample();

// clang-format on

bool rizz__vfs_init(const sx_alloc* alloc);
void rizz__vfs_release();
void rizz__vfs_async_update();

bool rizz__asset_init(const sx_alloc* alloc, const char* dbfile, const char* variation);
bool rizz__asset_save_meta_cache();
bool rizz__asset_dump_unused(const char* filepath);
void rizz__asset_release();
void rizz__asset_update();

bool rizz__gfx_init(const sx_alloc* alloc, const sg_desc* desc, bool enable_profile);
void rizz__gfx_release();
void rizz__gfx_trace_reset_frame_stats(rizz_gfx_perframe_trace_zone zone);
void rizz__gfx_execute_command_buffers_final();
void rizz__gfx_update();
void rizz__gfx_commit_gpu();

bool rizz__refl_init(const sx_alloc* alloc, int max_regs sx_default(0));
void rizz__refl_release();

// clang-format off
#define rizz__refl_enum(_type, _name)                    \
        the__refl._reg(RIZZ_REFL_ENUM, (void*)(intptr_t)_name, #_type, #_name, NULL, "", sizeof(_type), 0)
#define rizz__refl_func(_type, _name, _desc)             \
        the__refl._reg(RIZZ_REFL_FUNC, &_name, #_type, #_name, NULL, _desc, sizeof(void*), 0)
#define rizz__refl_field(_struct, _type, _name, _desc)   \
        the__refl._reg(RIZZ_REFL_FIELD, &(((_struct*)0)->_name), #_type, #_name, #_struct, _desc, sizeof(_type), sizeof(_struct))
// clang-format on

bool rizz__http_init(const sx_alloc* alloc);
void rizz__http_release();
void rizz__http_update();

typedef struct sg_desc sg_desc;
void rizz__app_init_gfx_desc(sg_desc* desc);
const void* rizz__app_d3d11_device(void);
const void* rizz__app_d3d11_device_context(void);

#ifdef __cplusplus
}
#endif