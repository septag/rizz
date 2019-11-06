#pragma once

#include "sx/platform.h"

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
#elif RIZZ_GRAPHICS_API_GLES == 30
#    define RIZZ_GRAPHICS_SHADER_LANG gles3
#elif RIZZ_GRAPHICS_API_GLES == 21
#    define RIZZ_GRAPHICS_SHADER_LANG gles2
#elif RIZZ_GRAPHICS_API_GL
#    define RIZZ_GRAPHICS_SHADER_LANG glsl
#endif

// default API variable names: the_core/the_app/...
// if you want a different name, set this macro before including this file
#ifndef RIZZ_APP_API_VARNAME
#    define RIZZ_APP_API_VARNAME the_app
#endif

#ifndef RIZZ_CORE_API_VARNAME
#    define RIZZ_CORE_API_VARNAME the_core
#endif

#ifndef RIZZ_ASSET_API_VARNAME
#    define RIZZ_ASSET_API_VARNAME the_asset
#endif

#ifndef RIZZ_CAMERA_API_VARNAME
#    define RIZZ_CAMERA_API_VARNAME the_camera
#endif

#ifndef RIZZ_GRAPHICS_API_VARNAME
#    define RIZZ_GRAPHICS_API_VARNAME the_gfx
#endif

#ifndef RIZZ_PLUGIN_API_VARNAME
#    define RIZZ_PLUGIN_API_VARNAME the_plugin
#endif

#ifndef RIZZ_VFS_API_VARNAME
#    define RIZZ_VFS_API_VARNAME the_vfs
#endif

#ifndef RIZZ_REFLECT_API_VARNAME
#    define RIZZ_REFLECT_API_VARNAME the_refl
#endif
