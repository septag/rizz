#pragma once

#include "sx/platform.h"

#if SX_PLATFORM_IOS
const char* rizz_ios_cache_dir();
const char* rizz_ios_data_dir();
void* rizz_ios_open_bundle(const char* name);
void rizz_ios_close_bundle(void* bundle);
void rizz_ios_resolve_path(void* bundle, const char* filepath, char* out_path, int out_path_sz);
#endif