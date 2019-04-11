//
// Copyright 2019 Sepehr Taghdisian (septag@github). All rights reserved.
// License: https://github.com/septag/rizz#license-bsd-2-clause
//
#pragma once

#include "types.h"

typedef struct sx_alloc sx_alloc;

#ifndef RIZZ_BUNDLE
// RIZZ_STATE: must put before any static variable that needs to be persistant in plugin reloads
// RIZZ_POINTER: must put before any function pointer that is passed to the engine like coroutines
// Reformatted from 'cr.h'
#    if SX_COMPILER_MSVC
#        pragma section(".state", read, write)
#        define RIZZ_STATE __declspec(allocate(".state"))
#        pragma section(".ptrs", read, write)
#        define RIZZ_POINTER __declspec(allocate(".ptrs"))
#    elif SX_PLATFORM_APPLE
#        define RIZZ_STATE __attribute__((used, section("__DATA,__state")))
#        define RIZZ_POINTER __attribute__((used, section("__DATA,__ptrs")))
#    elif SX_COMPILER_GCC || SX_COMPILER_CLANG
#        pragma section(".state", read, write)
#        define RIZZ_STATE __attribute__((section(".state")))
#        pragma section(".ptrs", read, write)
//#        define RIZZ_POINTER __attribute__((section(".ptrs")))
#        define RIZZ_POINTER    // not working yet, see: https://github.com/fungos/cr/issues/32
#    endif

// RIZZ_PLUGIN_EXPORT
#    if SX_COMPILER_MSVC
#        ifdef __cplusplus
#            define RIZZ_PLUGIN_EXPORT extern "C" __declspec(dllexport)
#        else
#            define RIZZ_PLUGIN_EXPORT __declspec(dllexport)
#        endif
#    else
#        ifdef __cplusplus
#            define RIZZ_PLUGIN_EXPORT extern "C" __attribute__((visibility("default")))
#        else
#            define RIZZ_PLUGIN_EXPORT __attribute__((visibility("default")))
#        endif
#    endif    // else: SX_COMPILER_MSVC
#else
#    ifdef __cplusplus
#        define RIZZ_PLUGIN_EXPORT extern "C"
#    else
#        define RIZZ_PLUGIN_EXPORT
#    endif
#    define RIZZ_STATE
#    define RIZZ_POINTER
#endif    // RIZZ_BUNDLE

typedef enum rizz_api_type {
    RIZZ_API_CORE = 0,
    RIZZ_API_PLUGIN,
    RIZZ_API_APP,
    RIZZ_API_GFX,
    RIZZ_API_REFLECT,
    RIZZ_API_VFS,
    RIZZ_API_ASSET,
    RIZZ_API_CAMERA,
    RIZZ_API_HTTP,
    _RIZZ_API_NATIVE_COUNT,
    RIZZ_API_IMGUI,
    RIZZ_API_IMGUI_EXTRA,
    RIZZ_API_SPRITE,
    _RIZZ_API_COUNT
} rizz_api_type;

typedef enum rizz_plugin_event {
    RIZZ_PLUGIN_EVENT_LOAD = 0,
    RIZZ_PLUGIN_EVENT_STEP = 1,
    RIZZ_PLUGIN_EVENT_UNLOAD = 2,
    RIZZ_PLUGIN_EVENT_SHUTDOWN = 3,
    RIZZ_PLUGIN_EVENT_INIT = 4
} rizz_plugin_event;

typedef enum rizz_plugin_crash {
    RIZZ_PLUGIN_CRASH_NONE = 0,    // No error
    RIZZ_PLUGIN_CRASH_SEGFAULT,    // SIGSEGV / EXCEPTION_ACCESS_VIOLATION
    RIZZ_PLUGIN_CRASH_ILLEGAL,     // illegal instruction (SIGILL) / EXCEPTION_ILLEGAL_INSTRUCTION
    RIZZ_PLUGIN_CRASH_ABORT,       // abort (SIGBRT)
    RIZZ_PLUGIN_CRASH_MISALIGN,    // bus error (SIGBUS) / EXCEPTION_DATATYPE_MISALIGNMENT
    RIZZ_PLUGIN_CRASH_BOUNDS,      // EXCEPTION_ARRAY_BOUNDS_EXCEEDED
    RIZZ_PLUGIN_CRASH_STACKOVERFLOW,        // EXCEPTION_STACK_OVERFLOW
    RIZZ_PLUGIN_CRASH_STATE_INVALIDATED,    // one or more global data sectio changed and does
                                            // not safely match basically a failure of
                                            // cr_plugin_validate_sections
    RIZZ_PLUGIN_CRASH_BAD_IMAGE,            // Binary is not valid
    RIZZ_PLUGIN_CRASH_OTHER,                // Unknown or other signal,
    RIZZ_PLUGIN_CRASH_USER = 0x100,
} rizz_plugin_crash;

enum rizz_plugin_info_flags_ { RIZZ_PLUGIN_INFO_EVENT_HANDLER = 0x1 };
typedef uint32_t rizz_plugin_info_flags;

// Plugins should implement these functions (names should be the same without the _cb)
// use rizz_plugin_decl macros to declare the proper function signatures based on build type
// (bundle/dynamic)
typedef struct rizz_plugin      rizz_plugin;
typedef struct rizz_plugin_info rizz_plugin_info;
typedef struct rizz_app_event   rizz_app_event;
typedef int(rizz_plugin_main_cb)(rizz_plugin* ctx, rizz_plugin_event e);
typedef void(rizz_plugin_get_info_cb)(rizz_plugin_info* out_info);
typedef void(rizz_plugin_event_handler_cb)(const rizz_app_event* ev);

typedef struct rizz_plugin_info {
    uint32_t               version;
    rizz_plugin_info_flags flags;
    char                   name[32];
    char                   desc[256];

#ifdef RIZZ_BUNDLE
    // These callback functions are automatically assigned by auto-generated script (see
    // bundle.cmake)
    rizz_plugin_main_cb*          main_cb;
    rizz_plugin_event_handler_cb* event_cb;
#endif
} rizz_plugin_info;

enum rizz_plugin_flags_ {
    RIZZ_PLUGIN_FLAG_MANUAL_RELOAD = 0x01,
    RIZZ_PLUGIN_FLAG_UPDATE_LAST = 0x02,
    RIZZ_PLUGIN_FLAG_UPDATE_FIRST = 0x04
};
typedef uint32_t rizz_plugin_flags;

typedef struct rizz_api_plugin {
    bool (*load)(const char* plugin_name, rizz_plugin_flags flags);
    void (*unload)(const char* plugin_name);
    void (*inject_api)(rizz_api_type type, uint32_t version, void* api);
    void (*remove_api)(rizz_api_type type, uint32_t version);
    void* (*get_api)(rizz_api_type api, uint32_t version);
    const char* (*crash_reason)(rizz_plugin_crash crash);
} rizz_api_plugin;

// Data layout is same as 'cr_plugin' but with different variable names to make it more
// user-friendly
typedef struct rizz_plugin {
    void*             _p;
    rizz_api_plugin*  api;
    uint32_t          iteration;    // What reload we are on, first load is 1
    rizz_plugin_crash crash_reason;
} rizz_plugin;

typedef struct rizz_app_event rizz_app_event;

#ifndef RIZZ_BUNDLE
#    define rizz_plugin_decl_main(_name, _plugin_param_name, _event_param_name)       \
        RIZZ_PLUGIN_EXPORT int rizz_plugin_main(rizz_plugin*      _plugin_param_name, \
                                                rizz_plugin_event _event_param_name)

#    define rizz_plugin_decl_event_handler(_name, __event_param_name) \
        RIZZ_PLUGIN_EXPORT void rizz_plugin_event_handler(const rizz_app_event* __event_param_name)

#    define rizz_plugin_implement_info(_name, _version, _desc, _flags)             \
        RIZZ_PLUGIN_EXPORT void rizz_plugin_get_info(rizz_plugin_info* out_info) { \
            out_info->version = (_version);                                        \
            out_info->flags = (_flags);                                            \
            sx_strcpy(out_info->name, sizeof(out_info->name), #_name);             \
            sx_strcpy(out_info->desc, sizeof(out_info->desc), (_desc));            \
        }
#else
#    define rizz_plugin_decl_main(_name, _plugin_param_name, _event_param_name)               \
        RIZZ_PLUGIN_EXPORT int rizz_plugin_main_##_name(rizz_plugin*      _plugin_param_name, \
                                                        rizz_plugin_event _event_param_name)
#    define rizz_plugin_decl_event_handler(_name, __event_param_name) \
        RIZZ_PLUGIN_EXPORT void rizz_plugin_event_handler_##_name(    \
            const rizz_app_event* __event_param_name)

#    define rizz_plugin_implement_info(_name, _version, _desc, _flags)                     \
        RIZZ_PLUGIN_EXPORT void rizz_plugin_get_info_##_name(rizz_plugin_info* out_info) { \
            out_info->version = (_version);                                                \
            out_info->flags = (_flags);                                                    \
            sx_strcpy(out_info->name, sizeof(out_info->name), #_name);                     \
            sx_strcpy(out_info->desc, sizeof(out_info->desc), (_desc));                    \
        }
#endif    // RIZZ_BUNDLE

#ifdef __cplusplus
extern "C" {
#endif

#ifdef RIZZ_INTERNAL_API
bool rizz__plugin_init(const sx_alloc* alloc, const char* plugin_path);
void rizz__plugin_release();
void rizz__plugin_broadcast_event(const rizz_app_event* e);
void rizz__plugin_update(float dt);
bool rizz__plugin_load_abs(const char* filepath, rizz_plugin_flags plugin_flags);
#endif

RIZZ_API rizz_api_plugin the__plugin;

#ifdef __cplusplus
}
#endif
