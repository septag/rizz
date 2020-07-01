//
// Copyright 2019 Sepehr Taghdisian (septag@github). All rights reserved.
// License: https://github.com/septag/rizz#license-bsd-2-clause
//

#include "internal.h"

#include "rizz/ios.h"

#include "sx/cmdline.h"
#include "sx/os.h"
#include "sx/string.h"

#include <stdio.h>

#include "Remotery.h"

// Choose api based on the platform
#if RIZZ_GRAPHICS_API_D3D==11
#    define SOKOL_D3D11
#elif RIZZ_GRAPHICS_API_METAL==1
#    define SOKOL_METAL
#elif RIZZ_GRAPHICS_API_GLES==21
#    define SOKOL_GLES2
#elif RIZZ_GRAPHICS_API_GLES==30
#    define SOKOL_GLES3
#elif RIZZ_GRAPHICS_API_GL==33
#    define SOKOL_GLCORE33
#else
#    error "Platform graphics is not supported"
#endif

#ifdef RIZZ_BUNDLE
#    include "plugin_bundle.h"
#endif

typedef struct {
    rizz_config conf;
    const sx_alloc* alloc;
    char game_filepath[RIZZ_MAX_PATH];
    sx_vec2 window_size;
    bool keys_pressed[RIZZ_APP_MAX_KEYCODES];
} rizz__app;

static rizz__app g_app;

SX_PRAGMA_DIAGNOSTIC_PUSH();
SX_PRAGMA_DIAGNOSTIC_IGNORED_CLANG_GCC("-Wunused-function")
static void* rizz__calloc(const sx_alloc* alloc, size_t num, size_t size)
{
    void* p = sx_malloc(alloc, num * size);
    if (p)
        sx_memset(p, 0x0, num * size);
    return p;
}
SX_PRAGMA_DIAGNOSTIC_POP();

// clang-format off
#define SOKOL_MALLOC(s)     sx_malloc(g_app.alloc, s)
#define SOKOL_FREE(p)       sx_free(g_app.alloc, p)
#define SOKOL_CALLOC(n, s)  rizz__calloc(g_app.alloc ? g_app.alloc : sx_alloc_malloc(), n, s)
#define SOKOL_ASSERT(c)     sx_assert(c)
#define SOKOL_LOG(s)        rizz__log_debug(s)
// clang-format on

SX_PRAGMA_DIAGNOSTIC_PUSH();
SX_PRAGMA_DIAGNOSTIC_IGNORED_CLANG_GCC("-Wunused-function")
#if SX_COMPILER_CLANG >= 60000
SX_PRAGMA_DIAGNOSTIC_IGNORED_CLANG_GCC("-Wlogical-not-parentheses")
#endif
SX_PRAGMA_DIAGNOSTIC_IGNORED_CLANG_GCC("-Wunused-parameter")
#define SOKOL_IMPL
#include "sokol/sokol_app.h"
SX_PRAGMA_DIAGNOSTIC_POP();

#define rizz__app_save_config_str(_cache_str, _str)      \
    if ((_str)) {                                        \
        sx_strcpy(_cache_str, sizeof(_cache_str), _str); \
        _str = _cache_str;                               \
    } else {                                             \
        _str = _cache_str;                               \
    }

#ifdef RIZZ_BUNDLE
RIZZ_PLUGIN_EXPORT void rizz_game_config(rizz_config*, int argc, char* argv[]);
#endif

static void rizz__app_init(void)
{
#if SX_PLATFORM_ANDROID
    static char default_cache_path[512];
    sx_strcpy(default_cache_path, sizeof(default_cache_path), rizz_android_cache_dir());
    g_app.conf.cache_path = default_cache_path;

    rizz_android_window_size(&g_app.conf.window_width, &g_app.conf.window_height);
    g_app.window_size = sx_vec2f(g_app.conf.window_width, g_app.conf.window_height);
#elif SX_PLATFORM_IOS
    static char default_cache_path[512];
    sx_strcpy(default_cache_path, sizeof(default_cache_path), rizz_ios_cache_dir());
    g_app.conf.cache_path = default_cache_path;

    g_app.conf.window_width = the__app.width();
    g_app.conf.window_height = the__app.height();
    g_app.window_size = sx_vec2f(g_app.conf.window_width, g_app.conf.window_height);
#elif SX_PLATFORM_RPI
    g_app.conf.window_width = the__app.width();
    g_app.conf.window_height = the__app.height();
    g_app.window_size = sx_vec2f(g_app.conf.window_width, g_app.conf.window_height);
#endif

    // Initialize engine components
    if (!rizz__core_init(&g_app.conf)) {
        rizz__log_error("core init failed");
        exit(-1);
    }

    // add game plugins
    int num_plugins = 0;
    for (int i = 0; i < RIZZ_CONFIG_MAX_PLUGINS; i++) {
        if (!g_app.conf.plugins[i] || !g_app.conf.plugins[i][0])
            break;

        if (!the__plugin.load(g_app.conf.plugins[i])) {
            exit(-1);
        }
        ++num_plugins;
    }

    // add game
#ifndef RIZZ_BUNDLE
    if (!rizz__plugin_load_abs(g_app.game_filepath, true, g_app.conf.plugins, num_plugins)) {
        rizz__log_error("loading game plugin failed: %s", g_app.game_filepath);
        exit(-1);
    }
#else
    if (!rizz__plugin_load_abs(ENTRY_NAME, true, g_app.conf.plugins, num_plugins)) {
        rizz__log_error("loading game plugin failed: %s", g_app.game_filepath);
        exit(-1);
    }
#endif

    // initialize all plugins
    if (!rizz__plugin_init_plugins()) {
        rizz__log_error("initializing plugins failed");
        exit(-1);
    }

    // At this point we can change the current directory if it's set
    if (g_app.conf.cwd && g_app.conf.cwd[0]) {
        sx_os_chdir(g_app.conf.cwd);
    }
}

static void rizz__app_frame(void)
{
    rizz__core_frame();
}

static void rizz__app_cleanup(void)
{
    rizz__core_release();
}

static void rizz__app_event(const sapp_event* e)
{
    static_assert(sizeof(rizz_app_event) == sizeof(sapp_event),
                  "sapp_event is not identical to rizz_app_event");
    static_assert(_RIZZ_APP_EVENTTYPE_NUM == _SAPP_EVENTTYPE_NUM,
                  "rizz_app_event_type does not match sokol");
    static_assert(offsetof(sapp_event, framebuffer_height) ==
                      offsetof(rizz_app_event, framebuffer_height),
                  "sapp_event is not identical to rizz_app_event");
    static_assert(sizeof(sapp_event) == sizeof(rizz_app_event),
                  "sapp_event is not identical to rizz_app_event");

    switch (e->type) {
    case RIZZ_APP_EVENTTYPE_RESIZED:
        g_app.conf.window_width = e->window_width;
        g_app.conf.window_height = e->window_height;
        g_app.window_size = sx_vec2f((float)e->window_width, (float)e->window_height);
        break;
    case RIZZ_APP_EVENTTYPE_SUSPENDED:
        break;
    case RIZZ_APP_EVENTTYPE_RESUMED:
        break;
    case RIZZ_APP_EVENTTYPE_KEY_DOWN:
        g_app.keys_pressed[e->key_code] = true;
        break;
    case RIZZ_APP_EVENTTYPE_KEY_UP:
        g_app.keys_pressed[e->key_code] = false;
        break;
    default:
        break;
    }

    // broadcast to plugins
    rizz__plugin_broadcast_event((const rizz_app_event*)e);
}

static void rizz__app_fail(const char* msg)
{
    rizz__log_error(msg);
}

static void rizz__app_show_help(sx_cmdline_context* cmdline)
{
    char buff[4096];
    sx_cmdline_create_help_string(cmdline, buff, sizeof(buff));
    puts(buff);
}

// Program's main entry point
sapp_desc sokol_main(int argc, char* argv[])
{
    g_app.alloc = sx_alloc_malloc();

    int profile_gpu = 0, dump_unused_assets = 0;

#ifndef RIZZ_BUNDLE
    int version = 0, show_help = 0;
    const sx_cmdline_opt opts[] = {
        { "version", 'V', SX_CMDLINE_OPTYPE_FLAG_SET, &version, 1, "Print version", 0x0 },
        { "run", 'r', SX_CMDLINE_OPTYPE_REQUIRED, 0x0, 'r', "Game/App module to run", "filepath" },
        { "profile-gpu", 'g', SX_CMDLINE_OPTYPE_FLAG_SET, &profile_gpu, 1, "Enable gpu profiler", 0x0 },
        { "cwd", 'c', SX_CMDLINE_OPTYPE_REQUIRED, 0x0, 'c', "Change current directory after initialization", 0x0 },
        { "dump-unused-assets", 'U', SX_CMDLINE_OPTYPE_FLAG_SET, &dump_unused_assets, 1, "Dump unused assets into `unused-assets.json`", 0x0 },
        { "help", 'h', SX_CMDLINE_OPTYPE_FLAG_SET, &show_help, 1, "Show this help message", 0x0 },
        SX_CMDLINE_OPT_END
    };
    sx_cmdline_context* cmdline =
        sx_cmdline_create_context(g_app.alloc, argc, (const char**)argv, opts);

    int opt;
    const char* arg;
    const char* game_filepath = NULL;
    const char* cwd = NULL;

    while ((opt = sx_cmdline_next(cmdline, NULL, &arg)) != -1) {
        switch (opt) {
        case '+':
            printf("Got argument without flag: %s\n", arg);
            break;
        case '?':
            printf("Unknown argument: %s\n", arg);
            exit(-1);
            break;
        case '!':
            printf("Invalid use of argument: %s\n", arg);
            exit(-1);
            break;
        case 'r':
            game_filepath = arg;
            break;
        case 'c':
            cwd = arg;
            break;
        default:
            break;
        }
    }

    if (show_help) {
        rizz__app_show_help(cmdline);
        exit(0);
    }

    if (game_filepath == NULL) {
        puts("provide a game module to run (--run)");
        exit(-1);
    }
    if (!sx_os_path_isfile(game_filepath)) {
        printf("Game module '%s' does not exist\n", game_filepath);
        exit(-1);
    }

    void* game_dll = sx_os_dlopen(game_filepath);
    if (game_dll == NULL) {
        printf("Game module '%s' is not a valid DLL: %s\n", game_filepath, sx_os_dlerr());
        exit(-1);
    }

    rizz_game_config_cb* game_config_fn =
        (rizz_game_config_cb*)sx_os_dlsym(game_dll, "rizz_game_config");
    if (!game_config_fn) {
        printf("Symbol 'rizz_game_config' not found in game module: %s\n", game_filepath);
        exit(-1);
    }
#else
    const char* game_filepath = argc > 0 ? argv[0] : "";
    rizz_game_config_cb* game_config_fn = rizz_game_config;
#endif    // RIZZ_BUNDLE

    static char default_name[64];
    static char default_title[64];
    static char default_html5_canvas[64] = { 0 };
    static char default_plugin_path[256] = { 0 };
    static char default_cache_path[256] = { 0 };
    static char default_plugins[32][RIZZ_CONFIG_MAX_PLUGINS] = {{ 0 }};
    static char default_cwd[256] = { 0 };

    char ext[16];
    sx_os_path_basename(default_name, sizeof(default_name), game_filepath);
    sx_os_path_splitext(ext, sizeof(ext), default_name, sizeof(default_name), default_name);
    sx_strcpy(default_title, sizeof(default_title), default_name);

    // default cache path:
    //      - win/linux/osx: executable-dir/.cache
    //      - android/ios: app cache dir (TODO)
#if !SX_PLATFORM_ANDROID && !SX_PLATFORM_IOS
    sx_os_path_exepath(default_cache_path, sizeof(default_cache_path));
#endif
    sx_os_path_dirname(default_cache_path, sizeof(default_cache_path), default_cache_path);
    sx_os_path_join(default_cache_path, sizeof(default_cache_path), default_cache_path, ".cache");

    // Create default config
    rizz_config conf = { .app_name = default_name,
                         .app_title = default_name,
                         .plugin_path = "",
                         .cwd = cwd,
                         .app_version = 1000,
                         .app_flags = RIZZ_APP_FLAG_USER_CURSOR,
                         .core_flags = 0,
                         .log_level = RIZZ_LOG_LEVEL_INFO,
                         .window_width = 640,
                         .window_height = 480,
                         .multisample_count = 1,
                         .swap_interval = 1,
                         .job_num_threads = -1,    // defaults to num_cores-1
                         .job_max_fibers = 64,
                         .job_stack_size = 1024,
                         .coro_max_fibers = 64,
                         .coro_stack_size = 2048,
                         .profiler_listen_port = 17815,    // default remotery port
                         .profiler_update_interval_ms = 10 };

    if (profile_gpu)
        conf.core_flags |= RIZZ_CORE_FLAG_PROFILE_GPU;
    if (dump_unused_assets)
        conf.core_flags |= RIZZ_CORE_FLAG_DUMP_UNUSED_ASSETS;
#ifdef _DEBUG
    conf.core_flags |= RIZZ_CORE_FLAG_DETECT_LEAKS;
#endif

    game_config_fn(&conf, argc, argv);

    // create .cache directory if it doesn't exist
    if (!conf.cache_path && !sx_os_path_isdir(default_cache_path))
        sx_os_mkdir(default_cache_path);

    // Before closing the dll, save the strings into static variables
    rizz__app_save_config_str(default_name, conf.app_name);
    rizz__app_save_config_str(default_title, conf.app_title);
    rizz__app_save_config_str(default_html5_canvas, conf.html5_canvas_name);
    rizz__app_save_config_str(default_plugin_path, conf.plugin_path);
    rizz__app_save_config_str(default_cache_path, conf.cache_path);
    rizz__app_save_config_str(default_cwd, conf.cwd);
    for (int i = 0; i < RIZZ_CONFIG_MAX_PLUGINS; i++) {
        if (conf.plugins[i]) {
            rizz__app_save_config_str(default_plugins[i], conf.plugins[i]);
        }
    }

#ifndef RIZZ_BUNDLE
    sx_os_dlclose(game_dll);
    sx_cmdline_destroy_context(cmdline, g_app.alloc);
#endif

    sx_memcpy(&g_app.conf, &conf, sizeof(rizz_config));
    sx_strcpy(g_app.game_filepath, sizeof(g_app.game_filepath), game_filepath);
    g_app.window_size = sx_vec2f((float)conf.window_width, (float)conf.window_height);

    return (
        sapp_desc){ .init_cb = rizz__app_init,
                    .frame_cb = rizz__app_frame,
                    .cleanup_cb = rizz__app_cleanup,
                    .event_cb = rizz__app_event,
                    .fail_cb = rizz__app_fail,
                    .width = conf.window_width,
                    .height = conf.window_height,
                    .sample_count = conf.multisample_count,
                    .window_title = conf.app_title,
                    .swap_interval = conf.swap_interval,
                    .high_dpi = (conf.app_flags & RIZZ_APP_FLAG_HIGHDPI) ? true : false,
                    .fullscreen = (conf.app_flags & RIZZ_APP_FLAG_FULLSCREEN) ? true : false,
                    .alpha = (conf.app_flags & RIZZ_APP_FLAG_ALPHA) ? true : false,
                    .html5_canvas_name = conf.html5_canvas_name,
                    .ios_keyboard_resizes_canvas =
                        (conf.app_flags & RIZZ_APP_FLAG_IOS_KEYBOARD_RESIZES_CANVAS) ? true : false,
                    .user_cursor = (conf.app_flags & RIZZ_APP_FLAG_USER_CURSOR) ? true : false,
                    .gl_force_gles2 = (conf.app_flags & RIZZ_APP_FLAG_FORCE_GLES2) ? true : false };
}

static sx_vec2 rizz__app_sizef(void)
{
    return g_app.window_size;
}

static const char* rizz__app_name(void)
{
    return g_app.conf.app_name;
}

static bool rizz__app_key_pressed(rizz_keycode key)
{
    return g_app.keys_pressed[key];
}

void rizz__app_init_gfx_desc(sg_desc* desc)
{
    sx_assert(sapp_isvalid());

    sx_memset(desc, 0x0, sizeof(sg_desc));
    sg_context_desc* context = &desc->context;

    context->gl.force_gles2 = sapp_gles2();
    context->metal.device = sapp_metal_get_device();
    context->metal.renderpass_descriptor_cb = sapp_metal_get_renderpass_descriptor;
    context->metal.drawable_cb = sapp_metal_get_drawable;
    context->d3d11.device = sapp_d3d11_get_device();
    context->d3d11.device_context = sapp_d3d11_get_device_context();
    context->d3d11.render_target_view_cb = sapp_d3d11_get_render_target_view;
    context->d3d11.depth_stencil_view_cb = sapp_d3d11_get_depth_stencil_view;
}

const void* rizz__app_d3d11_device(void)
{
    return sapp_d3d11_get_device();
}

const void* rizz__app_d3d11_device_context(void)
{
    return sapp_d3d11_get_device_context();
}

void* sapp_android_get_native_window(void)
{
#if SX_PLATFORM_ANDROID
    return _sapp_android_state.current.window;
#else
    return NULL;
#endif
}

static void rizz__app_mouse_capture(void)
{
#if SX_PLATFORM_WINDOWS
    SetCapture(_sapp_win32_hwnd);
#endif
}

static void rizz__app_mouse_release(void)
{
#if SX_PLATFORM_WINDOWS
    ReleaseCapture();
#endif
}

rizz_api_app the__app = { .width = sapp_width,
                          .height = sapp_height,
                          .sizef = rizz__app_sizef,
                          .highdpi = sapp_high_dpi,
                          .dpiscale = sapp_dpi_scale,
                          .show_keyboard = sapp_show_keyboard,
                          .keyboard_shown = sapp_keyboard_shown,
                          .name = rizz__app_name,
                          .key_pressed = rizz__app_key_pressed,
                          .quit = sapp_quit,
                          .request_quit = sapp_request_quit,
                          .cancel_quit = sapp_cancel_quit,
                          .show_mouse = sapp_show_mouse,
                          .mouse_shown = sapp_mouse_shown,
                          .mouse_capture = rizz__app_mouse_capture,
                          .mouse_release = rizz__app_mouse_release };
