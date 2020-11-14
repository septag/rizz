//
// Copyright 2019 Sepehr Taghdisian (septag@github). All rights reserved.
// License: https://github.com/septag/rizz#license-bsd-2-clause
//
#include "internal.h"

#include "sx/array.h"
#include "sx/io.h"
#include "sx/lockless.h"
#include "sx/os.h"
#include "sx/string.h"
#include "sx/threads.h"

#if SX_PLATFORM_ANDROID
#    include <android/asset_manager.h>
#    include <android/asset_manager_jni.h>
#    include <jni.h>
#endif

typedef enum {
    VFS_COMMAND_READ,    //
    VFS_COMMAND_WRITE    //
} rizz__vfs_async_command;

typedef enum {
    VFS_RESPONSE_READ_FAILED,
    VFS_RESPONSE_READ_OK,
    VFS_RESPONSE_WRITE_FAILED,
    VFS_RESPONSE_WRITE_OK
} rizz__vfs_response_code;

typedef struct {
    rizz__vfs_async_command cmd;
    rizz_vfs_flags flags;
    char path[RIZZ_MAX_PATH];
    sx_mem_block* write_mem;
    const sx_alloc* alloc;
    union {
        rizz_vfs_async_read_cb* read_fn;
        rizz_vfs_async_write_cb* write_fn;
    };
    void* user;
} rizz__vfs_async_request;

typedef struct {
    rizz__vfs_response_code code;
    union {
        sx_mem_block* read_mem;
        sx_mem_block* write_mem;
    };
    union {
        rizz_vfs_async_read_cb* read_fn;
        rizz_vfs_async_write_cb* write_fn;
    };
    void* user;
    int64_t write_bytes;    // on writes, it's the number of written bytes. on reads, it's -1
    char path[RIZZ_MAX_PATH];
} rizz__vfs_async_response;

typedef struct {
    char path[RIZZ_MAX_PATH];
    char alias[RIZZ_MAX_PATH];
    int alias_len;
#if RIZZ_CONFIG_HOT_LOADING
    uint32_t watch_id;
#endif
} rizz__vfs_mount_point;

typedef struct {
    const sx_alloc* alloc;
    rizz__vfs_mount_point* mounts;
    rizz_vfs_async_modify_cb** modify_cbs;    // sx_array
    sx_thread* worker_thrd;
    sx_queue_spsc* req_queue;    // producer: main, consumer: worker, data: rizz__vfs_async_request
    sx_queue_spsc* res_queue;    // producer: worker, consumer: main, data: rizz__vfs_async_response
    sx_sem worker_sem;
    int quit;

#if RIZZ_CONFIG_HOT_LOADING
    sx_queue_spsc* dmon_queue;    // producer: efsw_cb, consumer: main, data: efsw__result
#endif

#if SX_PLATFORM_IOS || SX_PLATFORM_ANDROID
    char assets_alias[RIZZ_MAX_PATH];
    int assets_alias_len;
#endif

#if SX_PLATFORM_IOS
    void* assets_bundle;
#endif

#if SX_PLATFORM_ANDROID
    AAssetManager* asset_mgr;
#endif
} rizz__vfs;

static rizz__vfs g_vfs;

#if RIZZ_CONFIG_HOT_LOADING
#    define DMON_IMPL
#    define DMON_MALLOC(size) sx_malloc(g_vfs.alloc, size)
#    define DMON_FREE(ptr) sx_free(g_vfs.alloc, ptr)
#    define DMON_REALLOC(ptr, size) sx_realloc(g_vfs.alloc, ptr, size)
#    include <stdio.h>
#    define DMON_LOG_ERROR(s) rizz__log_error(s)
#    ifndef NDEBUG
#        define DMON_LOG_DEBUG(s) rizz__log_debug(s)
#    else
#        define DMON_LOG_DEBUG(s)
#    endif
#    define DMON_MAX_PATH RIZZ_MAX_PATH
SX_PRAGMA_DIAGNOSTIC_PUSH()
SX_PRAGMA_DIAGNOSTIC_IGNORED_MSVC(5105)
#    include "dmon/dmon.h"
SX_PRAGMA_DIAGNOSTIC_POP()
#    if SX_PLATFORM_ANDROID || SX_PLATFORM_IOS
#        error "RIZZ_CONFIG_HOT_LOADING will not work under iOS/Android"
#    endif

static void dmon__event_cb(dmon_watch_id watch_id, dmon_action action, const char* rootdir,
                           const char* filepath, const char* oldfilepath, void* user);

typedef struct dmon__result {
    dmon_action action;
    char path[RIZZ_MAX_PATH];
} dmon__result;
#endif    // RIZZ_CONFIG_HOT_LOADING

static bool rizz__vfs_resolve_path(char* out_path, int out_path_sz, const char* path,
                                   rizz_vfs_flags flags)
{
    if (flags & RIZZ_VFS_FLAG_ABSOLUTE_PATH) {
        sx_os_path_normpath(out_path, out_path_sz, path);
        return true;
    } else {
        // search mount points and see if we find the path
        for (int i = 0, c = sx_array_count(g_vfs.mounts); i < c; i++) {
            const rizz__vfs_mount_point* mp = &g_vfs.mounts[i];
            if (sx_strnequal(path, mp->alias, mp->alias_len)) {
                char tmp_path[RIZZ_MAX_PATH];
                sx_os_path_normpath(tmp_path, sizeof(tmp_path), path + mp->alias_len);
                sx_os_path_join(out_path, out_path_sz, mp->path, tmp_path);
                return true;
            }
        }

        // check absolute path
        sx_os_path_normpath(out_path, out_path_sz, path);
        return sx_os_stat(out_path).type != SX_FILE_TYPE_INVALID;
    }
}

#if SX_PLATFORM_ANDROID
static sx_mem_block* rizz__vfs_read_asset_android(const char* path, const sx_alloc* alloc)
{
    if (path[0] == '/')
        ++path;
    AAsset* asset = AAssetManager_open(g_vfs.asset_mgr, path, AASSET_MODE_UNKNOWN);
    if (!asset)
        return NULL;

    off_t size = AAsset_getLength(asset);
    sx_mem_block* mem = NULL;
    int r = -1;
    if (size > 0) {
        mem = sx_mem_create_block(alloc, (int)size, NULL, 0);
        if (mem)
            r = AAsset_read(asset, mem->data, size);
    }
    AAsset_close(asset);

    if (mem && r > 0) {
        return mem;
    } else if (!mem) {
        return NULL;
    } else {
        sx_mem_destroy_block(mem);
        return NULL;
    }
}
#endif // SX_PLATFORM_ANDROID

static sx_mem_block* rizz__vfs_read(const char* path, rizz_vfs_flags flags, const sx_alloc* alloc)
{
    if (!alloc)
        alloc = g_vfs.alloc;

    char resolved_path[RIZZ_MAX_PATH];
#if SX_PLATFORM_ANDROID
    if (sx_strnequal(path, g_vfs.assets_alias, g_vfs.assets_alias_len)) {
        return rizz__vfs_read_asset_android(path + g_vfs.assets_alias_len, alloc);
    }
#elif SX_PLATFORM_IOS
    if (sx_strnequal(path, g_vfs.assets_alias, g_vfs.assets_alias_len)) {
        rizz_ios_resolve_path(g_vfs.assets_bundle, path + g_vfs.assets_alias_len, resolved_path,
                              sizeof(resolved_path));
    } else {
        rizz__vfs_resolve_path(resolved_path, sizeof(resolved_path), path, flags);
    }
#else
    rizz__vfs_resolve_path(resolved_path, sizeof(resolved_path), path, flags);
#endif

    return !(flags & RIZZ_VFS_FLAG_TEXT_FILE) ? sx_file_load_bin(alloc, resolved_path)
                                              : sx_file_load_text(alloc, resolved_path);
}

static int64_t rizz__vfs_write(const char* path, const sx_mem_block* mem, rizz_vfs_flags flags)
{
#if SX_PLATFORM_ANDROID
    if (sx_strnequal(path, g_vfs.assets_alias, g_vfs.assets_alias_len)) {
        sx_assertf(0, "cannot write to assets on mobile platform");
        return -1;
    }
#endif

    char resolved_path[RIZZ_MAX_PATH];
    sx_file f;

    rizz__vfs_resolve_path(resolved_path, sizeof(resolved_path), path, flags);
    bool r = sx_file_open(&f, resolved_path, SX_FILE_WRITE|SX_FILE_NOCACHE|
                          ((flags & RIZZ_VFS_FLAG_APPEND) ? SX_FILE_APPEND : 0));
    if (r) {
        int64_t written = sx_file_write(&f, mem->data, mem->size);
        sx_file_close(&f);
        return written;
    } else {
        return -1;
    }
}

static int rizz__vfs_worker(void* user1, void* user2)
{
    sx_unused(user1);
    sx_unused(user2);
    while (!g_vfs.quit) {
        rizz__vfs_async_request req;
        if (sx_queue_spsc_consume(g_vfs.req_queue, &req)) {
            rizz__vfs_async_response res = { .write_bytes = -1 };
            sx_strcpy(res.path, sizeof(res.path), req.path);
            res.user = req.user;

            switch (req.cmd) {
            case VFS_COMMAND_READ: {
                res.read_fn = req.read_fn;
                sx_mem_block* mem = rizz__vfs_read(req.path, req.flags, req.alloc);

                if (mem) {
                    res.code = VFS_RESPONSE_READ_OK;
                    res.read_mem = mem;
                } else {
                    res.code = VFS_RESPONSE_READ_FAILED;
                }
                sx_queue_spsc_produce_and_grow(g_vfs.res_queue, &res, g_vfs.alloc);
                break;
            }

            case VFS_COMMAND_WRITE: {
                res.write_fn = req.write_fn;
                int64_t written = rizz__vfs_write(req.path, req.write_mem, req.flags);

                if (written > 0) {
                    res.code = VFS_RESPONSE_WRITE_OK;
                    res.write_bytes = written;
                    res.write_mem = req.write_mem;
                } else {
                    res.code = VFS_RESPONSE_WRITE_FAILED;
                }
                sx_queue_spsc_produce_and_grow(g_vfs.res_queue, &res, g_vfs.alloc);
                break;
            }
            }
        }    // if (queue_consume)

        // wait on more jobs
        sx_semaphore_wait(&g_vfs.worker_sem, -1);
    }

    return 0;
}

bool rizz__vfs_mount(const char* path, const char* alias)
{
    if (sx_os_path_isdir(path)) {
        rizz__vfs_mount_point mp;
        sx_os_path_normpath(mp.path, sizeof(mp.path), path);
        sx_os_path_unixpath(mp.alias, sizeof(mp.alias), alias);
        mp.alias_len = sx_strlen(mp.alias);

#if RIZZ_CONFIG_HOT_LOADING
        mp.watch_id = dmon_watch(mp.path, dmon__event_cb, DMON_WATCHFLAGS_RECURSIVE, NULL).id;
#endif

        // check that the mount path is not already registered
        for (int i = 0, c = sx_array_count(g_vfs.mounts); i < c; i++) {
            if (sx_strequal(g_vfs.mounts[i].path, mp.path)) {
                rizz__log_error("vfs: path '%s' is already mounted on '%s'", mp.path, mp.alias);
                return false;
            }
        }


        sx_array_push(g_vfs.alloc, g_vfs.mounts, mp);
        rizz__log_info("vfs: mounted '%s' on '%s'", mp.alias, mp.path);
        return true;
    } else {
        rizz__log_error("mount path is not valid: %s", path);
        return false;
    }
}

void rizz__vfs_mount_mobile_assets(const char* alias)
{
    sx_unused(alias);
#if SX_PLATFORM_ANDROID || SX_PLATFORM_IOS
    sx_os_path_unixpath(g_vfs.assets_alias, sizeof(g_vfs.assets_alias), alias);
    g_vfs.assets_alias_len = sx_strlen(g_vfs.assets_alias);
    rizz__log_info("vfs: mounted '%s' on app assets", g_vfs.assets_alias);
#endif
}

bool rizz__vfs_init(const sx_alloc* alloc)
{
    g_vfs.alloc = alloc;

    g_vfs.req_queue = sx_queue_spsc_create(alloc, sizeof(rizz__vfs_async_request), 128);
    g_vfs.res_queue = sx_queue_spsc_create(alloc, sizeof(rizz__vfs_async_response), 128);
    if (!g_vfs.req_queue || !g_vfs.res_queue)
        return false;

    // create async worker thread and work queue
    sx_semaphore_init(&g_vfs.worker_sem);
    g_vfs.worker_thrd =
        sx_thread_create(alloc, rizz__vfs_worker, NULL, 1024 * 1024, "rizz_vfs", NULL);

#if RIZZ_CONFIG_HOT_LOADING
    dmon_init();

    g_vfs.dmon_queue = sx_queue_spsc_create(alloc, sizeof(dmon__result), 128);
    if (!g_vfs.dmon_queue)
        return false;
#endif // RIZZ_CONFIG_HOT_LOADING

#if SX_PLATFORM_ANDROID
    g_vfs.asset_mgr = rizz_android_asset_mgr();
#endif // SX_PLATFORM_ANDROID

#if SX_PLATFORM_IOS
    g_vfs.assets_bundle = rizz_ios_open_bundle("assets");
#endif // SX_PLATFORM_IOS

    return true;
}

void rizz__vfs_release()
{
    if (!g_vfs.alloc)
        return;

    if (g_vfs.worker_thrd) {
        g_vfs.quit = 1;
        sx_semaphore_post(&g_vfs.worker_sem, 1);
        sx_thread_destroy(g_vfs.worker_thrd, g_vfs.alloc);
        sx_semaphore_release(&g_vfs.worker_sem);
    }

    if (g_vfs.req_queue)
        sx_queue_spsc_destroy(g_vfs.req_queue, g_vfs.alloc);
    if (g_vfs.res_queue)
        sx_queue_spsc_destroy(g_vfs.res_queue, g_vfs.alloc);

#if RIZZ_CONFIG_HOT_LOADING
    dmon_deinit();

    if (g_vfs.dmon_queue) {
        sx_queue_spsc_destroy(g_vfs.dmon_queue, g_vfs.alloc);
    }
#endif // RIZZ_CONFIG_HOT_LOADING

    sx_array_free(g_vfs.alloc, g_vfs.modify_cbs);
    sx_array_free(g_vfs.alloc, g_vfs.mounts);
    g_vfs.alloc = NULL;
}

void rizz__vfs_async_update()
{
    // retreive results from worker thread and call the callback functions
    rizz__vfs_async_response res;
    while (sx_queue_spsc_consume(g_vfs.res_queue, &res)) {
        switch (res.code) {
        case VFS_RESPONSE_READ_OK:
        case VFS_RESPONSE_READ_FAILED:
            res.read_fn(res.path, res.read_mem, res.user);
            break;

        case VFS_RESPONSE_WRITE_OK:
        case VFS_RESPONSE_WRITE_FAILED:
            res.write_fn(res.path, res.write_bytes, res.write_mem, res.user);
            break;
        }
    }

#if RIZZ_CONFIG_HOT_LOADING
    // process efsw callbacks
    dmon__result dmon_res;
    while (sx_queue_spsc_consume(g_vfs.dmon_queue, &dmon_res)) {
        if (dmon_res.action == DMON_ACTION_MODIFY) {
            for (int i = 0, c = sx_array_count(g_vfs.modify_cbs); i < c; i++) {
                g_vfs.modify_cbs[i](dmon_res.path);
            }
        }
    }
#endif // RIZZ_CONFIG_HOT_LOADING
}

static void rizz__vfs_read_async(const char* path, rizz_vfs_flags flags, const sx_alloc* alloc,
                                 rizz_vfs_async_read_cb* read_fn, void* user)
{
    rizz__vfs_async_request req = { .cmd = VFS_COMMAND_READ,    //
                                    .flags = flags,
                                    .alloc = alloc,
                                    .read_fn = read_fn,
                                    .user = user };
    sx_strcpy(req.path, sizeof(req.path), path);
    sx_queue_spsc_produce_and_grow(g_vfs.req_queue, &req, g_vfs.alloc);
    sx_semaphore_post(&g_vfs.worker_sem, 1);
}

static void rizz__vfs_write_async(const char* path, sx_mem_block* mem, rizz_vfs_flags flags,
                                  rizz_vfs_async_write_cb* write_fn, void* user)
{
    rizz__vfs_async_request req = { .cmd = VFS_COMMAND_WRITE,
                                    .flags = flags,
                                    .write_mem = mem,
                                    .write_fn = write_fn,
                                    .user = user };
    sx_strcpy(req.path, sizeof(req.path), path);
    sx_queue_spsc_produce_and_grow(g_vfs.req_queue, &req, g_vfs.alloc);
    sx_semaphore_post(&g_vfs.worker_sem, 1);
}

static void rizz__vfs_register_modify(rizz_vfs_async_modify_cb* modify_cb)
{
    sx_assert(modify_cb);
    sx_array_push(g_vfs.alloc, g_vfs.modify_cbs, modify_cb);
}

static bool rizz__vfs_mkdir(const char* path)
{
    char resolved_path[RIZZ_MAX_PATH];
    if (rizz__vfs_resolve_path(resolved_path, sizeof(resolved_path), path, 0))
        return sx_os_mkdir(resolved_path);
    else
        return false;
}

static bool rizz__vfs_is_dir(const char* path)
{
    char resolved_path[RIZZ_MAX_PATH];
    if (rizz__vfs_resolve_path(resolved_path, sizeof(resolved_path), path, 0))
        return sx_os_path_isdir(resolved_path);
    else
        return false;
}

static bool rizz__vfs_is_file(const char* path)
{
    char resolved_path[RIZZ_MAX_PATH];
    if (rizz__vfs_resolve_path(resolved_path, sizeof(resolved_path), path, 0))
        return sx_os_path_isfile(resolved_path);
    else
        return false;
}

static uint64_t rizz__vfs_last_modified(const char* path)
{
    char resolved_path[RIZZ_MAX_PATH];
    if (rizz__vfs_resolve_path(resolved_path, sizeof(resolved_path), path, 0))
        return sx_os_stat(resolved_path).last_modified;
    else
        return 0;
}

#if RIZZ_CONFIG_HOT_LOADING
static void dmon__event_cb(dmon_watch_id watch_id, dmon_action action, const char* rootdir,
                           const char* filepath, const char* oldfilepath, void* user)
{
    sx_unused(oldfilepath);
    sx_unused(watch_id);
    sx_unused(user);

    switch (action) {
    case DMON_ACTION_MODIFY: {
        dmon__result r = (dmon__result){ r.action = action };
        char abs_filepath[RIZZ_MAX_PATH];
        sx_strcpy(abs_filepath, sizeof(abs_filepath), rootdir);
        sx_strcat(abs_filepath, sizeof(abs_filepath), filepath);
        sx_file_info info = sx_os_stat(abs_filepath);
        // TODO: when directory ignore added to dmon, remove this check
        if (info.type == SX_FILE_TYPE_REGULAR && info.size > 0) {
            for (int i = 0, c = sx_array_count(g_vfs.mounts); i < c; i++) {
                const rizz__vfs_mount_point* mp = &g_vfs.mounts[i];
                if (mp->watch_id == watch_id.id) {
                    sx_strcpy(r.path, sizeof(r.path), mp->alias);
                    int alias_len = sx_strlen(r.path);
                    if (alias_len > 0 && r.path[alias_len - 1] != '/') {
                        sx_assert((alias_len + 1) < (int)sizeof(r.path));
                        r.path[alias_len] = '/';
                        r.path[alias_len + 1] = '\0';
                    }

                    sx_strcat(r.path, sizeof(r.path), filepath);
                    break;
                }
            }

            if (r.path[0]) {
                sx_queue_spsc_produce_and_grow(g_vfs.dmon_queue, &r, g_vfs.alloc);
            }
        }
    } break;
    default:
        break;
    }
}
#endif // RIZZ_CONFIG_HOT_LOADING

rizz_api_vfs the__vfs = { .register_modify = rizz__vfs_register_modify,
                          .mount = rizz__vfs_mount,
                          .mount_mobile_assets = rizz__vfs_mount_mobile_assets,
                          .read_async = rizz__vfs_read_async,
                          .write_async = rizz__vfs_write_async,
                          .read = rizz__vfs_read,
                          .write = rizz__vfs_write,
                          .mkdir = rizz__vfs_mkdir,
                          .is_dir = rizz__vfs_is_dir,
                          .is_file = rizz__vfs_is_file,
                          .last_modified = rizz__vfs_last_modified };
