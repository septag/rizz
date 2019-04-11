//
// Copyright 2019 Sepehr Taghdisian (septag@github). All rights reserved.
// License: https://github.com/septag/rizz#license-bsd-2-clause
//
#pragma once

#include "app.h"
#include "plugin.h"

enum rizz_app_flags_ {
    RIZZ_APP_FLAG_HIGHDPI = 0x01,
    RIZZ_APP_FLAG_FULLSCREEN = 0x02,
    RIZZ_APP_FLAG_ALPHA = 0x04,
    RIZZ_APP_FLAG_PREMULTIPLIED_ALPHA = 0x08,
    RIZZ_APP_FLAG_PRESERVE_DRAWING_BUFFER = 0x10,
    RIZZ_APP_FLAG_HTML5_CANVAS_RESIZE = 0x20,
    RIZZ_APP_FLAG_IOS_KEYBOARD_RESIZES_CANVAS = 0x40,
    RIZZ_APP_FLAG_USER_CURSOR = 0x80,
    RIZZ_APP_FLAG_FORCE_GLES2 = 0x100
};
typedef uint32_t rizz_app_flags;

enum rizz_core_flags_ {
    RIZZ_CORE_FLAG_VERBOSE = 0x01,              // activate verbose logging
    RIZZ_CORE_FLAG_LOG_TO_FILE = 0x02,          // log to file defined by `app_name.log`
    RIZZ_CORE_FLAG_LOG_TO_PROFILER = 0x04,      // log to remote profiler
    RIZZ_CORE_FLAG_PROFILE_GPU = 0x08,          // enable GPU profiling
    RIZZ_CORE_FLAG_DUMP_UNUSED_ASSETS = 0x10    // write `unused-assets.json` on exit
};
typedef uint32_t rizz_core_flags;

// This structure can also be loaded from INI file
typedef struct rizz_config {
    const char*     app_name;
    const char*     app_title;
    const char*     plugin_path;
    const char*     cache_path;
    uint32_t        app_version;
    rizz_app_flags  app_flags;
    rizz_core_flags core_flags;

    int                window_width;
    int                window_height;
    int                multisample_count;
    int                swap_interval;
    const char*        html5_canvas_name;
    rizz_app_event_cb* event_cb;

    int job_num_threads;    // number of worker threads (default:-1, then it will be num_cores-1)
    int job_max_fibers;     // maximum running jobs at a time
    int job_stack_size;     // in kbytes

    int coro_max_fibers;    // maximum running (active) coroutines at a time
    int coro_stack_size;    // in kbytes

    int tmp_mem_max;    // in kbytes (defaut: 5mb per-thread)

    int profiler_listen_port;
    int profiler_update_interval_ms;    // default: 10
} rizz_config;

// Game plugins should implement this function (name should be "rizz_game_config")
// It is called by engine to fetch configuration before initializing the app
// The contents of conf is also set to defaults before submitting to this callback
typedef void(rizz_game_config_cb)(rizz_config* conf, int argc, char* argv[]);

#define rizz_game_decl_config(_conf_param_name) \
    RIZZ_PLUGIN_EXPORT void rizz_game_config(rizz_config* _conf_param_name, int argc, char* argv[])
