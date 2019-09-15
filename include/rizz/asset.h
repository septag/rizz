//
// Copyright 2019 Sepehr Taghdisian (septag@github). All rights reserved.
// License: https://github.com/septag/rizz#license-bsd-2-clause
//
// clang-format off
// Threading rules:
//      Internally, the asset system is offloading some work to worker threads, but the API is not
//      thread-safe 
//          1) Have to load all your assets on the main thread 
//          2) You can only use `rizz_api_asset.obj_threadsafe()` in worker threads.
//              So basically, you have to load the assets, and pass handles to threads, 
//              and they can only fetch the object pointer
//          3) Loading can be performed in the main thread while working threads are using the API 
//             (rule #2) but without RIZZ_ASSET_LOAD_FLAG_RELOAD flag 
//          4) Unloading can NOT be performed while working threads are using the API 
//          5) Never use asset objects across multiple frames inside worker-threads, 
//             because they may be invalidated
//
// So basically the multi-threading usage pattern is:
//      - Always Load your stuff in main update (main-thread) before running tasks that use those
//        assets
//      - in game-update function: spawn jobs and use `rizz_api_asset.obj_threadsafe()` to access
//        asset objects
//      - always wait/end these tasks before game-update functions ends
//      - unload assets only when the scene is not updated or no game-update tasks is running
// clang-format on
#pragma once

#include "sx/io.h"
#include "types.h"

enum rizz_asset_load_flags_ {
    RIZZ_ASSET_LOAD_FLAG_NONE = 0x0,
    RIZZ_ASSET_LOAD_FLAG_ABSOLUTE_PATH = 0x1,
    RIZZ_ASSET_LOAD_FLAG_WAIT_ON_LOAD = 0x2,
    RIZZ_ASSET_LOAD_FLAG_RELOAD = 0x4
};
typedef uint32_t rizz_asset_load_flags;

typedef struct rizz_asset_load_params {
    const char* path;               // path to asset file
    const void* params;             // must cast to asset-specific implementation type
    const sx_alloc* alloc;          // allocator that is user sends for loading asset data
    uint32_t tags;                  // user-defined tag bits
    rizz_asset_load_flags flags;    // flags that are used for loading
} rizz_asset_load_params;

typedef struct rizz_asset_load_data {
    rizz_asset_obj obj;    // valid internal object
    void* user;    // user-data can be allocated and filled with anything specific to asset loader
} rizz_asset_load_data;

typedef enum rizz_asset_state {
    RIZZ_ASSET_STATE_ZOMBIE = 0,
    RIZZ_ASSET_STATE_OK,
    RIZZ_ASSET_STATE_FAILED,
    RIZZ_ASSET_STATE_LOADING
} rizz_asset_state;

typedef struct rizz_asset_callbacks {
    // Runs on main-thread
    // Should create a valid object and any optional user-data.
    // 'metadata' is a custom structure defined by asset-loader, which stores important data to
    // prepare asset memory requirements each asset has it's own metadata
    rizz_asset_load_data (*on_prepare)(const rizz_asset_load_params* params, const void* metadata);

    // Runs on worker-thread
    // File data is loaded and passed as 'mem'. Should fill the allocated object and user-data.
    bool (*on_load)(rizz_asset_load_data* data, const rizz_asset_load_params* params,
                    const sx_mem_block* mem);

    // Runs on main-thread
    // Any optional finalization should happen in this function.
    // This function should free any user-data allocated in 'on_prepare' or 'on_load'
    void (*on_finalize)(rizz_asset_load_data* data, const rizz_asset_load_params* params,
                        const sx_mem_block* mem);

    // Runs on main-thread
    // Reloading of object happens automatically within asset-lib.
    // but this function can be used to sync any dependencies to entities or other assets
    // `prev_obj` is the previous object that is about to be replaced by the new one (the one in
    // `handle`)
    //            `prev_obj` is automatically released after this call by the asset manager
    void (*on_reload)(rizz_asset handle, rizz_asset_obj prev_obj, const sx_alloc* alloc);

    // Runs on main-thread
    // asset-lib calls this if asset's refcount reaches zero.
    void (*on_release)(rizz_asset_obj obj, const sx_alloc* alloc);

    // Runs on main-thread
    // If asset's metadata is not found, asset-lib calls this function to read them (and possibly
    // save it in cache). asset-loader should read headers or some other file data and fill
    // 'metadata' with valid information
    void (*on_read_metadata)(void* metadata, const rizz_asset_load_params* params,
                             const sx_mem_block* mem);
} rizz_asset_callbacks;

typedef struct rizz_api_asset {
    void (*register_asset_type)(const char* name, rizz_asset_callbacks callbacks,
                                const char* params_type_name, int params_size,
                                const char* metadata_type_name, int metadata_size,
                                rizz_asset_obj failed_obj, rizz_asset_obj async_obj,
                                rizz_asset_load_flags forced_flags);
    void (*unregister_asset_type)(const char* name);

    rizz_asset (*load)(const char* name, const char* path, const void* params,
                       rizz_asset_load_flags flags, const sx_alloc* alloc, uint32_t tags);
    rizz_asset (*load_from_mem)(const char* name, const char* path_alias, sx_mem_block* mem,
                                const void* params, rizz_asset_load_flags flags,
                                const sx_alloc* alloc, uint32_t tags);
    void (*unload)(rizz_asset handle);

    bool (*load_meta_cache)();

    rizz_asset_state (*state)(rizz_asset handle);
    const char* (*path)(rizz_asset handle);
    const char* (*type_name)(rizz_asset handle);
    const void* (*params)(rizz_asset handle);
    uint32_t (*tags)(rizz_asset handle);
    rizz_asset_obj (*obj)(rizz_asset handle);
    rizz_asset_obj (*obj_threadsafe)(rizz_asset handle);

    int (*ref_add)(rizz_asset handle);
    int (*ref_count)(rizz_asset handle);

    void (*reload_by_type)(const char* name);
    int (*gather_by_type)(const char* name, rizz_asset* out_handles, int max_handles);
    void (*unload_by_type)(const char* name);
    void (*reload_by_tags)(uint32_t tags);
    int (*gather_by_tags)(uint32_t tags, rizz_asset* out_handles, int max_handles);
    void (*unload_by_tags)(uint32_t tags);

    rizz_asset_group (*group_begin)(rizz_asset_group group);
    void (*group_end)();
    void (*group_wait)(rizz_asset_group group);
    bool (*group_loaded)(rizz_asset_group group);
    void (*group_delete)(rizz_asset_group group);
    void (*group_unload)(rizz_asset_group group);
    int (*group_gather)(rizz_asset_group group, rizz_asset* out_handles, int max_handles);
} rizz_api_asset;

#ifdef RIZZ_INTERNAL_API
bool rizz__asset_init(const sx_alloc* alloc, const char* dbfile, const char* variation);
bool rizz__asset_save_meta_cache();
bool rizz__asset_dump_unused(const char* filepath);
void rizz__asset_release();
void rizz__asset_update();

RIZZ_API rizz_api_asset the__asset;
#endif    // RIZZ_INTERNAL_API
