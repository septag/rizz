#pragma once

#include "rizz/types.h"

static inline char* ex_shader_path(char* path, int path_sz, const char* prefix_path, const char* filename) {
    sx_os_path_join(path, path_sz, prefix_path, sx_stringize(RIZZ_GRAPHICS_SHADER_LANG));
    sx_os_path_join(path, path_sz, path, filename);
    return sx_os_path_unixpath(path, path_sz, path);
}