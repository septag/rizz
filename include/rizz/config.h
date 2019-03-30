//
// Copyright 2019 Sepehr Taghdisian (septag@github). All rights reserved.
// License: https://github.com/septag/rizz#license-bsd-2-clause
//
#pragma once

#include "sx/platform.h"

#define RIZZ_VERSION "0.1.0-dev"

#ifndef SG_TYPES_ALREADY_DEFINED
enum {
    SG_INVALID_ID = 0,
    SG_NUM_SHADER_STAGES = 2,
    SG_NUM_INFLIGHT_FRAMES = 2,
    SG_MAX_COLOR_ATTACHMENTS = 4,
    SG_MAX_SHADERSTAGE_BUFFERS = 4,
    SG_MAX_SHADERSTAGE_IMAGES = 12,
    SG_MAX_SHADERSTAGE_UBS = 4,
    SG_MAX_UB_MEMBERS = 16,
    SG_MAX_VERTEX_ATTRIBUTES = 16,
    SG_MAX_MIPMAPS = 16,
    SG_MAX_TEXTUREARRAY_LAYERS = 128
};
#endif    // SG_TYPES_ALREADY_DEFINED

#ifndef RIZZ_MAX_PATH
#    define RIZZ_MAX_PATH 256
#endif    // RIZZ_MAX_PATH

// enables hot-loading for assets, plugins (+game) and shaders
#ifndef RIZZ_CONFIG_HOT_LOADING
#    if SX_PLATFORM_ANDROID || SX_PLATFORM_IOS
#        define RIZZ_CONFIG_HOT_LOADING 0
#    else
#        define RIZZ_CONFIG_HOT_LOADING 1
#    endif
#endif    // RIZZ_CONFIG_HOT_LOADING

// Time to query file changes of each plugin
#ifndef RIZZ_CONFIG_PLUGIN_UPDATE_INTERVAL
#    define RIZZ_CONFIG_PLUGIN_UPDATE_INTERVAL 1.0f
#endif

#ifndef RIZZ_CONFIG_ASSET_POOL_SIZE
#    define RIZZ_CONFIG_ASSET_POOL_SIZE 256
#endif

#ifndef RIZZ_CONFIG_MAX_HTTP_REQUESTS
#    define RIZZ_CONFIG_MAX_HTTP_REQUESTS 32
#endif

#ifndef RIZZ_CONFIG_MAX_DEBUG_VERTICES
#    define RIZZ_CONFIG_MAX_DEBUG_VERTICES 10000
#endif

#ifndef RIZZ_CONFIG_MAX_DEBUG_INDICES
#    define RIZZ_CONFIG_MAX_DEBUG_INDICES 10000
#endif

#ifndef RIZZ_CONFIG_DEBUG_MEMORY
#    ifdef _DEBUG
#        define RIZZ_CONFIG_DEBUG_MEMORY 1
#    else
#        define RIZZ_CONFIG_DEBUG_MEMORY 0
#    endif
#endif

// choose sokol api based on platform
#define RIZZ_GRAPHICS_API_D3D 0
#define RIZZ_GRAPHICS_API_METAL 0
#define RIZZ_GRAPHICS_API_GL 0
#define RIZZ_GRAPHICS_API_GLES 0

#if SX_PLATFORM_WINDOWS
#    undef RIZZ_GRAPHICS_API_D3D
#    define RIZZ_GRAPHICS_API_D3D 11
#elif SX_PLATFORM_APPLE
#    undef RIZZ_GRAPHICS_API_METAL
#    define RIZZ_GRAPHICS_API_METAL 1
#elif SX_PLATFORM_RPI || SX_PLATFORM_EMSCRIPTEN
#    undef RIZZ_GRAPHICS_API_GLES
#    define RIZZ_GRAPHICS_API_GLES 21
#elif SX_PLATFORM_ANDROID
#    undef RIZZ_GRAPHICS_API_GLES
#    define RIZZ_GRAPHICS_API_GLES 30
#elif SX_PLATFORM_LINUX
#    undef RIZZ_GRAPHICS_API_GL
#    define RIZZ_GRAPHICS_API_GL 33
#else
#    error "Platform graphics is not supported"
#endif

#if RIZZ_GRAPHICS_API_D3D
#    define RIZZ_GRAPHICS_SHADER_LANG hlsl
#elif RIZZ_GRAPHICS_API_METAL
#    define RIZZ_GRAPHICS_SHADER_LANG msl
#elif RIZZ_GRAPHICS_API_GLES
#    define RIZZ_GRAPHICS_SHADER_LANG gles
#elif RIZZ_GRAPHICS_API_GL
#    define RIZZ_GRAPHICS_SHADER_LANG glsl
#endif