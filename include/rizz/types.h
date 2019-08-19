//
// Copyright 2019 Sepehr Taghdisian (septag@github). All rights reserved.
// License: https://github.com/septag/rizz#license-bsd-2-clause
//
#pragma once

// clang-format off
#include "sx/sx.h"

#ifdef __cplusplus
#   define RIZZ_API extern "C" 
#else
#   define RIZZ_API extern
#endif

#define rizz_to_id(_index)     ((uint32_t)(_index) + 1)
#define rizz_to_index(_id)     ((int)(_id) - 1)

// versions are generally 4 digits: MNNF
// M: major
// NN: minor
// F: fix
#define rizz_version_major(_v)  ((_v)/1000)
#define rizz_version_minor(_v)  (((_v)%1000)/10)
#define rizz_version_bugfix(_v) ((_v)%10)

// asset.h
// Internal object that can be casted to the actual asset object, can be an Id or pointer based on asset
// asset loaders must always assume that 'zero' value is invalid, wether it's an Id or pointer
typedef union  { uintptr_t id;  void* ptr; }    rizz_asset_obj;

typedef struct { uint32_t id; } rizz_asset;
typedef struct { uint32_t id; } rizz_asset_group;
typedef struct { uint32_t id; } rizz_http;
typedef struct { uint32_t id; } rizz_gfx_stage;

#ifndef RIZZ_MAX_PATH
#    define RIZZ_MAX_PATH 256
#endif    // RIZZ_MAX_PATH


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
#elif RIZZ_GRAPHICS_API_GLES==30
#    define RIZZ_GRAPHICS_SHADER_LANG gles3
#elif RIZZ_GRAPHICS_API_GLES==21
#    define RIZZ_GRAPHICS_SHADER_LANG gles2
#elif RIZZ_GRAPHICS_API_GL
#    define RIZZ_GRAPHICS_SHADER_LANG glsl
#endif

// clang-format on