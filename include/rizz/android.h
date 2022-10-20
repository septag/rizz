#pragma once

#include "sx/platform.h"

#if SX_PLATFORM_ANDROID

#    include "rizz/rizz.h"
#    include <jni.h>

typedef struct rizz_android_method {
    JNIEnv* env;
    jclass clazz;
    jmethodID id;
} rizz_android_method;

typedef struct AAssetManager AAssetManager;

RIZZ_API bool rizz_android_get_method(rizz_android_method* method, const char* method_name,
                                      const char* method_sig, const char* class_path);
RIZZ_API bool rizz_android_get_static_method(rizz_android_method* method, const char* method_name,
                                             const char* method_sig, const char* class_path);
RIZZ_API const char* rizz_android_cache_dir();
RIZZ_API AAssetManager* rizz_android_asset_mgr();
RIZZ_API bool rizz_android_window_size(int* x, int* y);

#endif    // SX_PLATFORM_ANDROID