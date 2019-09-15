//
// Copyright 2019 Sepehr Taghdisian (septag@github). All rights reserved.
// License: https://github.com/septag/rizz#license-bsd-2-clause
//
#include "config.h"

#include "rizz/core.h"
#include "rizz/vfs.h"
#include "rizz/android.h"

#if RIZZ_CONFIG_HOT_LOADING
#    include "efsw/efsw.h"
typedef enum efsw_action efsw_action;
typedef enum efsw_error efsw_error;
#endif

#include "sx/allocator.h"
#include "sx/array.h"
#include "sx/io.h"
#include "sx/os.h"
#include "sx/string.h"
#include "sx/threads.h"
#include "sx/lockless.h"

#if SX_PLATFORM_ANDROID
#    include <android/asset_manager.h>
#    include <android/asset_manager_jni.h>
#    include <jni.h>
#endif

#if RIZZ_CONFIG_HOT_LOADING
static void efsw__fileaction_cb(efsw_watcher watcher, efsw_watchid watchid, const char* dir,
                                const char* filename, efsw_action action, const char* old_filename,
                                void* param);

typedef struct efsw__result {
    efsw_action action;
    char path[RIZZ_MAX_PATH];
} efsw__result;
#endif    // RIZZ_CONFIG_HOT_LOADING

typedef enum { VFS_COMMAND_READ, VFS_COMMAND_WRITE } rizz__vfs_async_command;

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
} rizz__vfs_async_request;

typedef struct {
    rizz__vfs_response_code code;
    union {
        sx_mem_block* read_mem;
        sx_mem_block* write_mem;
    };
    int write_bytes;    // on writes, it's the number of written bytes. on reads, it's 0
    char path[RIZZ_MAX_PATH];
} rizz__vfs_async_response;

typedef struct {
    char path[RIZZ_MAX_PATH];
    char alias[RIZZ_MAX_PATH];
    int alias_len;
#if RIZZ_CONFIG_HOT_LOADING
    char efsw_root_dir[RIZZ_MAX_PATH];
    efsw_watchid watch_id;
#endif
} rizz__vfs_mount_point;

typedef struct {
    const sx_alloc* alloc;
    rizz__vfs_mount_point* mounts;
    rizz_vfs_async_callbacks callbacks;
    sx_thread* worker_thrd;
    sx_queue_spsc* req_queue;    // producer: main, consumer: worker, data: rizz__vfs_async_request
    sx_queue_spsc* res_queue;    // producer: worker, consumer: main, data: rizz__vfs_async_response
    sx_sem worker_sem;
    int quit;

#if RIZZ_CONFIG_HOT_LOADING
    efsw_watcher watcher;
    sx_queue_spsc* efsw_queue;    // producer: efsw_cb, consumer: main, data: efsw__result
#endif

#if SX_PLATFORM_IOS
    int assets_bundle_id;
#endif

#if SX_PLATFORM_ANDROID
    AAssetManager* asset_mgr;
    char assets_alias[RIZZ_MAX_PATH];
    int assets_alias_len;
#endif
} rizz__vfs;

static rizz__vfs g_vfs;

// Dummy callbacks
void rizz__dummy_on_read_error(const char* uri)
{
    rizz_log_debug("disk: read_error: %s", uri);
}
void rizz__dummy_on_write_error(const char* uri)
{
    rizz_log_debug("disk: write_error: %s", uri);
}
void rizz__dummy_on_read_complete(const char* uri, sx_mem_block* mem)
{
    rizz_log_debug("disk: read_complete: %s (size: %d kb)", uri, mem->size / 1024);
    sx_mem_destroy_block(mem);
}
void rizz__dummy_on_write_complete(const char* uri, int bytes_written, sx_mem_block* mem)
{
    rizz_log_debug("disk: write_complete: %s (size: %d kb)", uri, bytes_written / 1024);
    sx_mem_destroy_block(mem);
}
void rizz__dummy_on_modified(const char* uri)
{
    rizz_log_debug("disk: modified: %s", uri);
}

static rizz_vfs_async_callbacks g_vfs_dummy_callbacks = {
    .on_read_error = rizz__dummy_on_read_error,
    .on_write_error = rizz__dummy_on_write_error,
    .on_read_complete = rizz__dummy_on_read_complete,
    .on_write_complete = rizz__dummy_on_write_complete,
    .on_modified = rizz__dummy_on_modified
};

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
#endif

static sx_mem_block* rizz__vfs_read(const char* path, rizz_vfs_flags flags, const sx_alloc* alloc)
{
    if (!alloc)
        alloc = g_vfs.alloc;

#if SX_PLATFORM_ANDROID
    if (sx_strnequal(path, g_vfs.assets_alias, g_vfs.assets_alias_len))
        return rizz__vfs_read_asset_android(path + g_vfs.assets_alias_len, alloc);
#endif

    char resolved_path[RIZZ_MAX_PATH];
    rizz__vfs_resolve_path(resolved_path, sizeof(resolved_path), path, flags);
    return !(flags & RIZZ_VFS_FLAG_TEXT_FILE) ? sx_file_load_bin(alloc, resolved_path)
                                              : sx_file_load_text(alloc, resolved_path);
}

static int rizz__vfs_write(const char* path, const sx_mem_block* mem, rizz_vfs_flags flags)
{
#if SX_PLATFORM_ANDROID
    if (sx_strnequal(path, g_vfs.assets_alias, g_vfs.assets_alias_len)) {
        sx_assert(0 && "cannot write to assets on mobile platform");
        return -1;
    }
#endif

    char resolved_path[RIZZ_MAX_PATH];
    sx_file_writer writer;

    rizz__vfs_resolve_path(resolved_path, sizeof(resolved_path), path, flags);
    bool r = sx_file_open_writer(&writer, resolved_path,
                                 !(flags & RIZZ_VFS_FLAG_APPEND) ? 0 : SX_FILE_OPEN_APPEND);
    if (r) {
        int written = sx_file_write(&writer, mem->data, mem->size);
        sx_file_close_writer(&writer);
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
            rizz__vfs_async_response res;
            res.read_mem = NULL;
            res.write_bytes = 0;
            sx_strcpy(res.path, sizeof(res.path), req.path);

            switch (req.cmd) {
            case VFS_COMMAND_READ: {
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
                int written = rizz__vfs_write(req.path, req.write_mem, req.flags);

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
        if (SX_PLATFORM_OSX) {
            sx_os_path_abspath(mp.efsw_root_dir, sizeof(mp.efsw_root_dir), mp.path);
        } else {
            sx_strcpy(mp.efsw_root_dir, sizeof(mp.efsw_root_dir), mp.path);
        }

        mp.watch_id = efsw_addwatch(g_vfs.watcher, mp.path, efsw__fileaction_cb, 1, NULL);
#endif

        // check that the mount path is not already registered
        for (int i = 0, c = sx_array_count(g_vfs.mounts); i < c; i++) {
            if (sx_strequal(g_vfs.mounts[i].path, mp.path)) {
                rizz_log_error("vfs: path '%s' is already mounted on '%s'", mp.path, mp.alias);
                return false;
            }
        }


        sx_array_push(g_vfs.alloc, g_vfs.mounts, mp);
        rizz_log_info("vfs: mounted '%s' on '%s'", mp.alias, mp.path);
        return true;
    } else {
        rizz_log_error("mount path is not valid: %s", path);
        return false;
    }
}

void rizz__vfs_mount_android_assets(const char* alias)
{
    sx_unused(alias);
#if SX_PLATFORM_ANDROID
    sx_os_path_unixpath(g_vfs.assets_alias, sizeof(g_vfs.assets_alias), alias);
    g_vfs.assets_alias_len = sx_strlen(g_vfs.assets_alias);
    rizz_log_info("vfs: mounted '%s' on android assets", g_vfs.assets_alias);
#endif
}

bool rizz__vfs_init(const sx_alloc* alloc)
{
    g_vfs.alloc = alloc;
    g_vfs.callbacks = g_vfs_dummy_callbacks;

    g_vfs.req_queue = sx_queue_spsc_create(alloc, sizeof(rizz__vfs_async_request), 128);
    g_vfs.res_queue = sx_queue_spsc_create(alloc, sizeof(rizz__vfs_async_response), 128);
    if (!g_vfs.req_queue || !g_vfs.res_queue)
        return false;

    // create async worker thread and work queue
    sx_semaphore_init(&g_vfs.worker_sem);
    g_vfs.worker_thrd =
        sx_thread_create(alloc, rizz__vfs_worker, NULL, 1024 * 1024, "rizz_vfs", NULL);

#if RIZZ_CONFIG_HOT_LOADING
    g_vfs.watcher = efsw_create(0);
    if (!g_vfs.watcher) {
        rizz_log_error("efsw: could not create context");
        return false;
    }

    g_vfs.efsw_queue = sx_queue_spsc_create(alloc, sizeof(efsw__result), 128);
    if (!g_vfs.efsw_queue)
        return false;
#endif    // RIZZ_CONFIG_HOT_LOADING

#if SX_PLATFORM_ANDROID
    g_vfs.asset_mgr = rizz_android_asset_mgr();
#endif

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
    if (g_vfs.watcher) {
        for (int i = 0, c = sx_array_count(g_vfs.mounts); i < c; i++) {
            rizz__vfs_mount_point* mp = &g_vfs.mounts[i];
            if (mp->watch_id)
                efsw_removewatch_byid(g_vfs.watcher, mp->watch_id);
        }
        efsw_release(g_vfs.watcher);
    }

    if (g_vfs.efsw_queue)
        sx_queue_spsc_destroy(g_vfs.efsw_queue, g_vfs.alloc);
#endif

    sx_array_free(g_vfs.alloc, g_vfs.mounts);
    g_vfs.alloc = NULL;
}

void rizz__vfs_watch_mounts()
{
#if RIZZ_CONFIG_HOT_LOADING
    if (sx_array_count(g_vfs.mounts)) {
        efsw_watch(g_vfs.watcher);
    }
#endif
}

void rizz__vfs_async_update()
{
    // retreive results from worker thread and call the callback functions
    rizz__vfs_async_response res;
    while (sx_queue_spsc_consume(g_vfs.res_queue, &res)) {
        switch (res.code) {
        case VFS_RESPONSE_READ_OK:
            g_vfs.callbacks.on_read_complete(res.path, res.read_mem);
            break;

        case VFS_RESPONSE_WRITE_OK:
            g_vfs.callbacks.on_write_complete(res.path, res.write_bytes, res.write_mem);
            break;

        case VFS_RESPONSE_READ_FAILED:
            g_vfs.callbacks.on_read_error(res.path);
            break;

        case VFS_RESPONSE_WRITE_FAILED:
            g_vfs.callbacks.on_write_error(res.path);
            break;
        }
    }

#if RIZZ_CONFIG_HOT_LOADING
    if (g_vfs.watcher) {
        // process efsw callbacks
        efsw__result efsw_res;
        while (sx_queue_spsc_consume(g_vfs.efsw_queue, &efsw_res)) {
            switch (efsw_res.action) {
            case EFSW_MODIFIED:
                g_vfs.callbacks.on_modified(efsw_res.path);
                break;
            default:
                sx_assert(0 && "not implemented");
                break;
            }
        }
    }
#endif
}

static void rizz__vfs_read_async(const char* path, rizz_vfs_flags flags, const sx_alloc* alloc)
{
    rizz__vfs_async_request req = { .cmd = VFS_COMMAND_READ, .flags = flags, .alloc = alloc };
    sx_strcpy(req.path, sizeof(req.path), path);
    sx_queue_spsc_produce_and_grow(g_vfs.req_queue, &req, g_vfs.alloc);
    sx_semaphore_post(&g_vfs.worker_sem, 1);
}

static void rizz__vfs_write_async(const char* path, sx_mem_block* mem, rizz_vfs_flags flags)
{
    rizz__vfs_async_request req = { .cmd = VFS_COMMAND_WRITE, .flags = flags, .write_mem = mem };
    sx_strcpy(req.path, sizeof(req.path), path);
    sx_queue_spsc_produce_and_grow(g_vfs.req_queue, &req, g_vfs.alloc);
    sx_semaphore_post(&g_vfs.worker_sem, 1);
}

static rizz_vfs_async_callbacks rizz__vfs_set_async_callbacks(const rizz_vfs_async_callbacks* cbs)
{
    rizz_vfs_async_callbacks prev = g_vfs.callbacks;
    if (cbs) {
        g_vfs.callbacks = *cbs;
    } else {
        g_vfs.callbacks = g_vfs_dummy_callbacks;
    }
    return prev;
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
static void efsw__fileaction_cb(efsw_watcher watcher, efsw_watchid watchid, const char* dir,
                                const char* filename, efsw_action action, const char* old_filename,
                                void* param)
{
    sx_unused(param);
    sx_unused(old_filename);
    sx_unused(watchid);
    sx_unused(watcher);

    switch (action) {
    case EFSW_MODIFIED: {
        efsw__result r = { .action = action };
        sx_os_path_join(r.path, sizeof(r.path), dir, filename);
        sx_file_info info = sx_os_stat(r.path);
        if (info.type == SX_FILE_TYPE_REGULAR && info.size > 0) {
            for (int i = 0, c = sx_array_count(g_vfs.mounts); i < c; i++) {
                const rizz__vfs_mount_point* mp = &g_vfs.mounts[i];
                if (sx_strstr(r.path, mp->efsw_root_dir) == r.path) {
                    char relpath[RIZZ_MAX_PATH];
                    sx_os_path_relpath(relpath, sizeof(relpath), r.path, mp->efsw_root_dir);
                    sx_os_path_join(r.path, sizeof(r.path), mp->alias, relpath);
                    if (SX_PLATFORM_WINDOWS)
                        sx_os_path_unixpath(r.path, sizeof(r.path), r.path);
                    break;
                }
            }

            if (r.path[0])
                sx_queue_spsc_produce_and_grow(g_vfs.efsw_queue, &r, g_vfs.alloc);
        }
        break;
    }
    case EFSW_DELETE:
    case EFSW_ADD:
    case EFSW_MOVED:
        break;
    }
}
#endif

rizz_api_vfs the__vfs = { .set_async_callbacks = rizz__vfs_set_async_callbacks,
                          .mount = rizz__vfs_mount,
                          .mount_android_assets = rizz__vfs_mount_android_assets,
                          .watch_mounts = rizz__vfs_watch_mounts,
                          .read_async = rizz__vfs_read_async,
                          .write_async = rizz__vfs_write_async,
                          .read = rizz__vfs_read,
                          .write = rizz__vfs_write,
                          .mkdir = rizz__vfs_mkdir,
                          .is_dir = rizz__vfs_is_dir,
                          .is_file = rizz__vfs_is_file,
                          .last_modified = rizz__vfs_last_modified };
