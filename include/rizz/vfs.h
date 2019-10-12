//
// Copyright 2019 Sepehr Taghdisian (septag@github). All rights reserved.
// License: https://github.com/septag/rizz#license-bsd-2-clause
//
#pragma once

#include "sx/sx.h"

typedef struct sx_mem_block sx_mem_block;
typedef struct sx_alloc sx_alloc;

enum rizz_vfs_flags_ {
    RIZZ_VFS_FLAG_NONE = 0x01,
    RIZZ_VFS_FLAG_ABSOLUTE_PATH = 0x02,
    RIZZ_VFS_FLAG_TEXT_FILE = 0x04,
    RIZZ_VFS_FLAG_APPEND = 0x08
};
typedef uint32_t rizz_vfs_flags;

typedef struct rizz_vfs_async_callbacks {
    void (*on_read_error)(const char* path);
    void (*on_write_error)(const char* path);
    void (*on_read_complete)(const char* path, sx_mem_block* mem);
    void (*on_write_complete)(const char* path, int bytes_written, sx_mem_block* mem);
    void (*on_modified)(const char* path);
} rizz_vfs_async_callbacks;

typedef struct rizz_api_vfs {
    rizz_vfs_async_callbacks (*set_async_callbacks)(const rizz_vfs_async_callbacks* cbs);
    bool (*mount)(const char* path, const char* alias);
    void (*mount_mobile_assets)(const char* alias);
    void (*watch_mounts)();
    void (*read_async)(const char* path, rizz_vfs_flags flags, const sx_alloc* alloc);
    void (*write_async)(const char* path, sx_mem_block* mem, rizz_vfs_flags flags);
    sx_mem_block* (*read)(const char* path, rizz_vfs_flags flags, const sx_alloc* alloc);
    int (*write)(const char* path, const sx_mem_block* mem, rizz_vfs_flags flags);
    bool (*mkdir)(const char* path);
    bool (*is_dir)(const char* path);
    bool (*is_file)(const char* path);
    uint64_t (*last_modified)(const char* path);
} rizz_api_vfs;

#ifdef RIZZ_INTERNAL_API
#    include "types.h"

bool rizz__vfs_init(const sx_alloc* alloc);
void rizz__vfs_release();
void rizz__vfs_async_update();

RIZZ_API rizz_api_vfs the__vfs;
#endif    // RIZZ_INTERNAL_API
