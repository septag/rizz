//
// Copyright 2019 Sepehr Taghdisian (septag@github). All rights reserved.
// License: https://github.com/septag/rizz#license-bsd-2-clause
//

#include "internal.h"

#include "rizz/ios.h"

#include "sx/cmdline.h"
#include "sx/os.h"
#include "sx/string.h"
#include "sx/array.h"
#include "sx/ini.h"

#include <stdio.h>

#include "Remotery.h"
#include "stackwalkerc/stackwalkerc.h"

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

typedef struct rizz__cmdline_item {
    char name[64];
    bool allocated_value;
    union {
        char  value[8];
        char* value_ptr;
    };
} rizz__cmdline_item;

typedef struct rizz__shortcut_item {
    rizz_keycode keys[2];       // maximum 2 keys are allowed
    rizz_modifier_keys mods;    // with any combination of modifier keys
    rizz_app_shortcut_cb* callback;
    void* user;
} rizz__shortcut_item;

typedef struct rizz__app {
    rizz_config conf;
    sx_ini* conf_ini;
    const sx_alloc* alloc;
    char game_filepath[RIZZ_MAX_PATH];
    sx_vec2 window_size;
    bool keys_pressed[RIZZ_APP_MAX_KEYCODES];
    sx_cmdline_opt* cmdline_args;       // sx_array
    rizz__cmdline_item* cmdline_items;  // saved items with values
    rizz__shortcut_item* shortcuts;     // sx_array
    void (*crash_cb)(void*, void*);
    void* crash_user_data;
    bool suspended;
    bool iconified;
} rizz__app;

static rizz__app g_app;

// default config fields
static char default_name[64];
static char default_title[64];
static char default_html5_canvas[64];
static char default_plugin_path[256];
static char default_cache_path[256];
static char default_plugins[32][RIZZ_CONFIG_MAX_PLUGINS];
static char default_cwd[256];

#define SOKOL_MALLOC(s)     sx_malloc(g_app.alloc, s)
#define SOKOL_FREE(p)       sx_free(g_app.alloc, p)
#define SOKOL_CALLOC(n, s)  sx_calloc(g_app.alloc ? g_app.alloc : sx_alloc_malloc(), n*s)
#define SOKOL_ASSERT(c)     sx_assert(c)
#define SOKOL_LOG(s)        rizz__log_debug(s)

SX_PRAGMA_DIAGNOSTIC_PUSH();
SX_PRAGMA_DIAGNOSTIC_IGNORED_CLANG_GCC("-Wunused-function")
#if SX_COMPILER_CLANG >= 60000
SX_PRAGMA_DIAGNOSTIC_IGNORED_CLANG_GCC("-Wlogical-not-parentheses")
#endif
SX_PRAGMA_DIAGNOSTIC_IGNORED_CLANG_GCC("-Wunused-parameter")
SX_PRAGMA_DIAGNOSTIC_IGNORED_MSVC(5105)
#define SOKOL_IMPL
#include "sokol/sokol_app.h"
SX_PRAGMA_DIAGNOSTIC_POP();

// crash dump is currently only supported on windows
#if SX_PLATFORM_WINDOWS
#    include <minidumpapiset.h>    // MiniDumpWriteDump
#endif                             // !RIZZ_FINAL

#define CONFIG_FILENAME "rizz.ini"

#define rizz__app_save_config_str(_cache_str, _str)      \
    if ((_str)) {                                        \
        sx_strcpy(_cache_str, sizeof(_cache_str), _str); \
        _str = _cache_str;                               \
    } else {                                             \
        _str = _cache_str;                               \
    }

#ifdef RIZZ_BUNDLE
RIZZ_PLUGIN_EXPORT void rizz_game_config(rizz_config*, rizz_register_cmdline_arg_cb*);
#endif

#if SX_PLATFORM_WINDOWS
static void rizz__app_message_box(const char* text)
{
    MessageBoxA(NULL, text, "rizz", MB_OK|MB_ICONERROR);
}

static long WINAPI rizz__app_generate_crash_dump(EXCEPTION_POINTERS* except)
{
    if (g_app.crash_cb) {
        g_app.crash_cb(except, g_app.crash_user_data);
    }

    #if !RIZZ_FINAL
    if (g_app.conf.app_flags & RIZZ_APP_FLAG_CRASH_DUMP) {
        HMODULE dbghelp = sw_load_dbghelp();
        if (dbghelp) {
            typedef BOOL (__stdcall* MiniDumpWriteDump_t)(HANDLE, DWORD, HANDLE, MINIDUMP_TYPE, PMINIDUMP_EXCEPTION_INFORMATION, 
                                                          PMINIDUMP_USER_STREAM_INFORMATION, PMINIDUMP_CALLBACK_INFORMATION);
            MiniDumpWriteDump_t fMiniDumpWriteDump = (MiniDumpWriteDump_t)GetProcAddress(dbghelp, "MiniDumpWriteDump");
            if (fMiniDumpWriteDump) {
                MINIDUMP_EXCEPTION_INFORMATION einfo = { 0 };
                HANDLE dump_handle;

                einfo.ThreadId = GetCurrentThreadId();
                einfo.ExceptionPointers = except;
                einfo.ClientPointers = TRUE;

                char filename[256];
                sx_strcat(sx_strcpy(filename, sizeof(filename), g_app.conf.app_name), sizeof(filename), "_crash.dmp");
                dump_handle = CreateFile(filename, GENERIC_READ | GENERIC_WRITE, 
                                         FILE_SHARE_WRITE | FILE_SHARE_READ, 0, CREATE_ALWAYS, 0, 0);
                fMiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), dump_handle, MiniDumpNormal, &einfo, NULL, NULL);

                rizz__log_error("Crash: dump created '%s'", filename);
            }

            FreeLibrary(dbghelp);
        }
    } 
    #endif

    char msg[256];
    sx_snprintf(
        msg, sizeof(msg), "Program exception: (Version: %d.%d-%s) Code: 0x%x, Address: 0x%p",
        the__core.version().major, the__core.version().minor, the__core.version().git,
        except->ExceptionRecord->ExceptionCode, except->ExceptionRecord->ExceptionAddress);
    rizz__log_error(msg);
    rizz__app_message_box(msg);

    return EXCEPTION_EXECUTE_HANDLER;
}

#else
static void rizz__app_message_box(const char* text)
{
    puts(text);
}
#endif

static uint32_t rizz__app_parse_flags(const char* flags_str, uint32_t(*convert_enum_cb)(const char* value))
{
    uint32_t flags = 0;
    char value[64];
    flags_str = sx_skip_whitespace(flags_str);
    while (flags_str[0]) {
        const char* next_pipe =  sx_strchar(flags_str, '|');
        if (next_pipe) {
            sx_strncpy(value, sizeof(value), flags_str, (int)(uintptr_t)(next_pipe - flags_str));
            sx_trim_whitespace(value, sizeof(value), value);
            flags_str = next_pipe + 1;

            flags |= convert_enum_cb(value);
        }
        const char* next_flag = sx_skip_whitespace(flags_str);
        if (next_flag == flags_str) {
            break;
        }
        flags_str = next_flag;
    }

    if (flags_str[0]) {
        sx_strcpy(value, sizeof(value), flags_str);
        sx_trim_whitespace(value, sizeof(value), value);
        flags |= convert_enum_cb(value);
    }

    return flags;
}

static uint32_t rizz__app_convert_app_flags(const char* value)
{
    if (sx_strequalnocase(value, "HIGHDPI")) {
        return RIZZ_APP_FLAG_HIGHDPI;
    } else if (sx_strequalnocase(value, "FULLSCREEN")) {
        return RIZZ_APP_FLAG_FULLSCREEN;
    } else if (sx_strequalnocase(value, "ALPHA")) {
        return RIZZ_APP_FLAG_ALPHA;
    } else if (sx_strequalnocase(value, "PREMULTIPLIED_ALPHA")) {
        return RIZZ_APP_FLAG_PREMULTIPLIED_ALPHA;
    } else if (sx_strequalnocase(value, "PRESERVE_DRAWING_BUFFER")) {
        return RIZZ_APP_FLAG_PRESERVE_DRAWING_BUFFER;
    } else if (sx_strequalnocase(value, "HTML5_CANVAS_RESIZE")) {
        return RIZZ_APP_FLAG_HTML5_CANVAS_RESIZE;
    } else if (sx_strequalnocase(value, "IOS_KEYBOARD_RESIZES_CANVAS")) {
        return RIZZ_APP_FLAG_IOS_KEYBOARD_RESIZES_CANVAS;
    } else if (sx_strequalnocase(value, "USER_CURSOR")) {
        return RIZZ_APP_FLAG_USER_CURSOR;
    } else if (sx_strequalnocase(value, "FORCE_GLES2")) {
        return RIZZ_APP_FLAG_FORCE_GLES2;
    } else {
        return 0;
    }
}

static uint32_t rizz__app_convert_core_flags(const char* value)
{
    if (sx_strequalnocase(value, "LOG_TO_FILE")) {
        return RIZZ_CORE_FLAG_LOG_TO_FILE;
    } else if (sx_strequalnocase(value, "LOG_TO_PROFILER")) {
        return RIZZ_CORE_FLAG_LOG_TO_PROFILER;
    } else if (sx_strequalnocase(value, "PROFILE_GPU")) {
        return RIZZ_CORE_FLAG_PROFILE_GPU;
    } else if (sx_strequalnocase(value, "DUMP_UNUSED_ASSETS")) {
        return RIZZ_CORE_FLAG_DUMP_UNUSED_ASSETS;
    } else if (sx_strequalnocase(value, "DETECT_LEAKS")) {
        return RIZZ_CORE_FLAG_DETECT_LEAKS;
    } else if (sx_strequalnocase(value, "DEBUG_TEMP_ALLOCATOR")) {
        return RIZZ_CORE_FLAG_DEBUG_TEMP_ALLOCATOR;
    } else {
        return 0;
    }
}

static rizz_log_level rizz__app_convert_log_level(const char* value)
{
    if (sx_strequalnocase(value, "ERROR")) {
        return RIZZ_LOG_LEVEL_ERROR;
    } else if (sx_strequalnocase(value, "WARNING")) {
        return RIZZ_LOG_LEVEL_WARNING;
    } else if (sx_strequalnocase(value, "INFO")) {
        return RIZZ_LOG_LEVEL_INFO;
    } else if (sx_strequalnocase(value, "VERBOSE")) {
        return RIZZ_LOG_LEVEL_VERBOSE;
    } else if (sx_strequalnocase(value, "DEBUG")) {
        return RIZZ_LOG_LEVEL_DEBUG;
    } else {
        return 0;
    }
}

static sg_filter rizz__app_convert_sg_filter(const char* value)
{
    if (sx_strequalnocase(value, "NEAREST"))    {
        return SG_FILTER_NEAREST;
    } else if (sx_strequalnocase(value, "LINEAR")) {
        return SG_FILTER_LINEAR;
    } else if (sx_strequalnocase(value, "NEAREST_MIPMAP_NEAREST")) {
        return SG_FILTER_NEAREST_MIPMAP_NEAREST;
    } else if (sx_strequalnocase(value, "NEAREST_MIPMAP_LINEAR")) {
        return SG_FILTER_NEAREST_MIPMAP_LINEAR;
    } else if (sx_strequalnocase(value, "LINEAR_MIPMAP_NEAREST")) {
        return SG_FILTER_LINEAR_MIPMAP_NEAREST;
    } else if (sx_strequalnocase(value, "LINEAR_MIPMAP_LINEAR")) {
        return SG_FILTER_LINEAR_MIPMAP_LINEAR;
    } else {
        return 0;
    }

}

static void rizz__app_load_ini(const char* filename, rizz_config* conf)
{
    sx_unused(filename);

    sx_mem_block* mem = sx_file_load_text(g_app.alloc, filename);
    if (mem) {
        sx_ini* ini = sx_ini_load(mem->data, g_app.alloc);
        if (ini) {
            g_app.conf_ini = ini;
            int rizz_id = sx_ini_find_section(ini, "rizz", 0);
            if (rizz_id != -1) {
                int id;
                id = sx_ini_find_property(ini, rizz_id, "app_name", 0);
                if (id != -1)
                    sx_strcpy(default_name, sizeof(default_name), sx_ini_property_value(ini, rizz_id, id));
                id = sx_ini_find_property(ini, rizz_id, "app_title", 0);
                if (id != -1)
                    sx_strcpy(default_title, sizeof(default_title), sx_ini_property_value(ini, rizz_id, id));
                id = sx_ini_find_property(ini, rizz_id, "plugin_path", 0);
                if (id != -1)
                    sx_strcpy(default_plugin_path, sizeof(default_plugin_path), sx_ini_property_value(ini, rizz_id, id));
                id = sx_ini_find_property(ini, rizz_id, "cache_path", 0);
                if (id != -1)
                    sx_strcpy(default_cache_path, sizeof(default_cache_path), sx_ini_property_value(ini, rizz_id, id));
                id = sx_ini_find_property(ini, rizz_id, "cwd", 0);
                if (id != -1)
                    sx_strcpy(default_cwd, sizeof(default_cwd), sx_ini_property_value(ini, rizz_id, id));
                id = sx_ini_find_property(ini, rizz_id, "app_flags", 0);
                if (id != -1)
                    conf->app_flags = rizz__app_parse_flags(sx_ini_property_value(ini, rizz_id, id), rizz__app_convert_app_flags);
                id = sx_ini_find_property(ini, rizz_id, "core_flags", 0);
                if (id != -1)
                    conf->core_flags = rizz__app_parse_flags(sx_ini_property_value(ini, rizz_id, id), rizz__app_convert_core_flags);
                id = sx_ini_find_property(ini, rizz_id, "log_level", 0);
                if (id != -1) 
                    conf->log_level = rizz__app_convert_log_level(sx_ini_property_value(ini, rizz_id, id));
                id = sx_ini_find_property(ini, rizz_id, "window_width", 0);
                if (id != -1) 
                    conf->window_width = sx_toint(sx_ini_property_value(ini, rizz_id, id));
                id = sx_ini_find_property(ini, rizz_id, "window_height", 0);
                if (id != -1)
                    conf->window_height = sx_toint(sx_ini_property_value(ini, rizz_id, id));
                id = sx_ini_find_property(ini, rizz_id, "window_height", 0);
                if (id != -1)
                    conf->window_height = sx_toint(sx_ini_property_value(ini, rizz_id, id));
                id = sx_ini_find_property(ini, rizz_id, "multisample_count", 0);
                if (id != -1)
                    conf->multisample_count = sx_toint(sx_ini_property_value(ini, rizz_id, id));
                id = sx_ini_find_property(ini, rizz_id, "swap_interval", 0);
                if (id != -1)
                    conf->swap_interval = sx_toint(sx_ini_property_value(ini, rizz_id, id));
                id = sx_ini_find_property(ini, rizz_id, "texture_first_mip", 0);
                if (id != -1) 
                    conf->texture_first_mip = sx_toint(sx_ini_property_value(ini, rizz_id, id));
                id = sx_ini_find_property(ini, rizz_id, "texture_aniso", 0);
                if (id != -1) 
                    conf->texture_aniso = sx_toint(sx_ini_property_value(ini, rizz_id, id));
                id = sx_ini_find_property(ini, rizz_id, "texture_filter_min", 0);
                if (id != -1) 
                    conf->texture_filter_min = rizz__app_convert_sg_filter(sx_ini_property_value(ini, rizz_id, id));
                id = sx_ini_find_property(ini, rizz_id, "texture_filter_mag", 0);
                if (id != -1) 
                    conf->texture_filter_mag = rizz__app_convert_sg_filter(sx_ini_property_value(ini, rizz_id, id));
                id = sx_ini_find_property(ini, rizz_id, "job_num_threads", 0);
                if (id != -1) 
                    conf->job_num_threads = sx_toint(sx_ini_property_value(ini, rizz_id, id));
                id = sx_ini_find_property(ini, rizz_id, "job_max_fibers", 0);
                if (id != -1)
                    conf->job_max_fibers = sx_toint(sx_ini_property_value(ini, rizz_id, id));
                id = sx_ini_find_property(ini, rizz_id, "job_stack_size", 0);
                if (id != -1)
                    conf->job_stack_size = sx_toint(sx_ini_property_value(ini, rizz_id, id));
                id = sx_ini_find_property(ini, rizz_id, "coro_num_init_fibers", 0);
                if (id != -1)
                    conf->coro_num_init_fibers = sx_toint(sx_ini_property_value(ini, rizz_id, id));
                id = sx_ini_find_property(ini, rizz_id, "coro_stack_size", 0);
                if (id != -1)
                    conf->coro_stack_size = sx_toint(sx_ini_property_value(ini, rizz_id, id));
                id = sx_ini_find_property(ini, rizz_id, "tmp_mem_max", 0);
                if (id != -1)
                    conf->tmp_mem_max = sx_toint(sx_ini_property_value(ini, rizz_id, id));
                id = sx_ini_find_property(ini, rizz_id, "profiler_listen_port", 0);
                if (id != -1)
                    conf->profiler_listen_port = sx_toint(sx_ini_property_value(ini, rizz_id, id));
                id = sx_ini_find_property(ini, rizz_id, "profiler_update_interval_ms", 0);
                if (id != -1)
                    conf->profiler_update_interval_ms = sx_toint(sx_ini_property_value(ini, rizz_id, id));
                id = sx_ini_find_property(ini, rizz_id, "imgui_docking", 0);
                if (id != -1) 
                    conf->imgui_docking = sx_tobool(sx_ini_property_value(ini, rizz_id, id));
            }
        }
        sx_mem_destroy_block(mem);
    }
}

static void rizz__app_init(void)
{
    char errmsg[512];

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
        rizz__app_message_box("core init failed, see log for details");
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
        sx_snprintf(errmsg, sizeof(errmsg), g_app.game_filepath);
        rizz__log_error(errmsg);
        rizz__app_message_box(errmsg);
        exit(-1);
    }
#else
    if (!rizz__plugin_load_abs(ENTRY_NAME, true, g_app.conf.plugins, num_plugins)) {
        sx_snprintf(errmsg, sizeof(errmsg), g_app.game_filepath);
        rizz__log_error(errmsg);
        rizz__app_message_box(errmsg);
        exit(-1);
    }
#endif

    // initialize all plugins
    if (!rizz__plugin_init_plugins()) {
        rizz__log_error("initializing plugins failed");
        rizz__app_message_box("initializing plugins failed, see log for details");
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

    for (int i = 0; i < sx_array_count(g_app.cmdline_items); i++) {
        if (g_app.cmdline_items[i].allocated_value) {
            sx_free(g_app.alloc, g_app.cmdline_items[i].value_ptr);
        }
    }
    sx_array_free(g_app.alloc, g_app.cmdline_items);

    if (g_app.conf_ini) {
        sx_ini_destroy(g_app.conf_ini);
    }
}

static void rizz__app_process_shortcuts(rizz_modifier_keys input_mods)
{
    for (int i = 0, c = sx_array_count(g_app.shortcuts); i < c; i++) {
        rizz_keycode key1 = g_app.shortcuts[i].keys[0];
        rizz_keycode key2 = g_app.shortcuts[i].keys[1];
        rizz_modifier_keys keymods = g_app.shortcuts[i].mods;

        if (g_app.keys_pressed[key1] && (key2 == SAPP_KEYCODE_INVALID || g_app.keys_pressed[key2]) &&
            (keymods == 0 || (keymods & input_mods))) {
            g_app.shortcuts[i].callback(g_app.shortcuts[i].user);
        } 
    }
}

static void rizz__app_event(const sapp_event* e)
{
    static_assert(sizeof(rizz_app_event) == sizeof(sapp_event), "sapp_event is not identical to rizz_app_event");
    static_assert(_RIZZ_APP_EVENTTYPE_NUM == _SAPP_EVENTTYPE_NUM, "rizz_app_event_type does not match sokol");
    static_assert(offsetof(sapp_event, framebuffer_height) == offsetof(rizz_app_event, framebuffer_height),
                  "sapp_event is not identical to rizz_app_event");
    static_assert(sizeof(sapp_event) == sizeof(rizz_app_event), "sapp_event is not identical to rizz_app_event");

    switch (e->type) {
    case RIZZ_APP_EVENTTYPE_RESIZED:
        g_app.conf.window_width = e->window_width;
        g_app.conf.window_height = e->window_height;
        g_app.window_size = sx_vec2f((float)e->window_width, (float)e->window_height);
        if (!g_app.iconified && !g_app.suspended && the__core.is_paused()) {
            the__core.resume();
        }
        break;
    case RIZZ_APP_EVENTTYPE_MOVED:
        if (!g_app.iconified && !g_app.suspended && the__core.is_paused()) {
            the__core.resume();
        }
        break;
    case RIZZ_APP_EVENTTYPE_SUSPENDED:
        the__core.pause();
        g_app.suspended = true;
        break;
    case RIZZ_APP_EVENTTYPE_ICONIFIED:
        if (!(g_app.conf.app_flags & RIZZ_APP_FLAG_RESUME_ICONIFIED)) {
            the__core.pause();
        }
        g_app.iconified = true;
        break;
    case RIZZ_APP_EVENTTYPE_RESUMED:
        the__core.resume();
        g_app.suspended = false;
        break;
    case RIZZ_APP_EVENTTYPE_RESTORED:
        if (the__core.is_paused()) {
            the__core.resume();
        }
        g_app.iconified = false;
        break;
    case RIZZ_APP_EVENTTYPE_MOVING:
    case RIZZ_APP_EVENTTYPE_RESIZING:
        if (!the__core.is_paused()) {
            the__core.pause();
        }
        break;
    case RIZZ_APP_EVENTTYPE_KEY_DOWN:
        g_app.keys_pressed[e->key_code] = true;
        rizz__app_process_shortcuts(e->modifiers);
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

static void rizz__app_write_help_tofile(const char* filename, sx_cmdline_context* cmdline, sx_cmdline_context* cmdline_app)
{
    char buff[4096];
    sx_cmdline_create_help_string(cmdline, buff, sizeof(buff));
    
    FILE* f = fopen(filename, "wt");
    if (f) {
        if (cmdline) {
            sx_cmdline_create_help_string(cmdline, buff, sizeof(buff));
            fputs(buff, f);
            fputs("", f);
        }

        if (cmdline_app) {
            sx_cmdline_create_help_string(cmdline_app, buff, sizeof(buff));
            fputs(buff, f);
            fputs("", f);
        }
        fclose(f);
    }
}


static sx_cmdline_context* rizz__app_parse_cmdline(int argc, char* argv[])
{
    sx_cmdline_opt end_opt = SX_CMDLINE_OPT_END;
    sx_array_push(g_app.alloc, g_app.cmdline_args, end_opt);
    sx_cmdline_context* cmdline = sx_cmdline_create_context(g_app.alloc, argc, (const char**)argv, g_app.cmdline_args);

    int opt;
    int index;
    const char* arg;
    int count = sx_array_count(g_app.cmdline_args)-1;
    while ((opt = sx_cmdline_next(cmdline, &index, &arg)) != -1) {
        for (int i = 0; i < count; i++) {
            // detect short or long name
            if (argv[index][0] == '-' && argv[index][1] == '-') {
                const char* eq = sx_strchar(argv[index], '=');
                if (eq) {
                    char _arg[64];
                    sx_strncpy(_arg, sizeof(_arg), &argv[index][2], (int)(uintptr_t)(eq - &argv[index][2]));
                    if (!sx_strequal(g_app.cmdline_args[i].name, _arg)) {
                        continue;
                    }
                } else if (!sx_strequal(g_app.cmdline_args[i].name, &argv[index][2])) {
                    continue;
                }
            } else if (argv[index][0] == '-') {
                if (g_app.cmdline_args[i].name_short != argv[index][2]) {
                    continue;
                }
            } else {
                continue;
            }

            rizz__cmdline_item item;
            sx_strcpy(item.name, sizeof(item.name), g_app.cmdline_args[i].name);

            if (arg) {
                int arg_len = sx_strlen(arg);
                if (arg_len > (int)sizeof(item.value) - 1) {
                    item.allocated_value = false;
                    sx_strcpy(item.value, sizeof(item.value), arg);
                } else {
                    item.allocated_value = true;
                    item.value_ptr = sx_malloc(g_app.alloc, arg_len + 1);
                    if (!item.value_ptr) {
                        sx_memory_fail();
                    } else {
                        sx_strcpy(item.value_ptr, arg_len + 1, arg);
                    }
                }
            } else {
                item.allocated_value = false;
                sx_snprintf(item.value, sizeof(item.value), "%d", opt);
            }
             
            sx_array_push(g_app.alloc, g_app.cmdline_items, item);
            break;
        }
    }

    return cmdline;
}

static void rizz__app_cmdline_arg(const char* name, char short_name, rizz_cmdline_argtype type,
                                  const char* desc, const char* value_desc)
{
    for (int i = 0; i < sx_array_count(g_app.cmdline_items); i++) {
        sx_cmdline_opt* opt = &g_app.cmdline_args[i];
        if (sx_strequal(opt->name, name)) {
            sx_assertf(0, "command-line argument '%s' is already registered", name);
            return;
        }
    }

    sx_cmdline_opt opt = (sx_cmdline_opt){ .name = name,
                                           .name_short = (int)short_name,
                                           .type = (sx_cmdline_optype)type,
                                           .value = 1,
                                           .desc = desc,
                                           .value_desc = value_desc };

    sx_array_push(g_app.alloc, g_app.cmdline_args, opt);
}


// Program's main entry point
sapp_desc sokol_main(int argc, char* argv[])
{
    g_app.alloc = sx_alloc_malloc();

    int profile_gpu = 0, dump_unused_assets = 0, crash_dump = 0;

    int version = 0, show_help = 0;
    const sx_cmdline_opt opts[] = {
        { "version", 'V', SX_CMDLINE_OPTYPE_FLAG_SET, &version, 1, "Print version", 0x0 },
        { "run", 'r', SX_CMDLINE_OPTYPE_REQUIRED, 0x0, 'r', "Game/App module to run", "filepath" },
        { "profile-gpu", 'g', SX_CMDLINE_OPTYPE_FLAG_SET, &profile_gpu, 1, "Enable gpu profiler", 0x0 },
        { "cwd", 'c', SX_CMDLINE_OPTYPE_REQUIRED, 0x0, 'c', "Change current directory after initialization", 0x0 },
        { "dump-unused-assets", 'U', SX_CMDLINE_OPTYPE_FLAG_SET, &dump_unused_assets, 1, "Dump unused assets into `unused-assets.json`", 0x0 },
        { "crash-dump", 'd', SX_CMDLINE_OPTYPE_FLAG_SET, &crash_dump, 1, "Create crash dump file on program exceptions", 0x0 },
        { "first-mip", 'M', SX_CMDLINE_OPTYPE_REQUIRED, 0x0, 'M', "Set the first mip of textures. Higher values lower texture sizes (default=0)", 0x0 },
        { "help", 'h', SX_CMDLINE_OPTYPE_FLAG_SET, &show_help, 1, "Show this help message", 0x0 },
        SX_CMDLINE_OPT_END
    };
    sx_cmdline_context* cmdline = sx_cmdline_create_context(g_app.alloc, argc, (const char**)argv, opts);

    int opt;
    const char* arg;
    const char* game_filepath = NULL;
    const char* cwd = NULL;
    char errmsg[512];
    int first_mip = 0;

    while ((opt = sx_cmdline_next(cmdline, NULL, &arg)) != -1) {
        switch (opt) {
        case '+':
            sx_snprintf(errmsg, sizeof(errmsg), "Got argument without flag: %s", arg);
            rizz__app_message_box(errmsg);
            break;
        case '!':
            sx_snprintf(errmsg, sizeof(errmsg), "Invalid use of argument: %s", arg);
            rizz__app_message_box(errmsg);
            exit(-1);
            break;
        case 'r':
            game_filepath = arg;
            break;
        case 'c':
            cwd = arg;
            break;
        case 'M':
            first_mip = sx_toint(arg);
            break;
        default:
            break;
        }
    }

#ifndef RIZZ_BUNDLE
    if (game_filepath == NULL) {
        rizz__app_message_box("provide a game module to run (--run)");
        exit(-1);
    }
    if (!sx_os_path_isfile(game_filepath)) {
        sx_snprintf(errmsg, sizeof(errmsg), "Game module '%s' does not exist", game_filepath);
        rizz__app_message_box(errmsg);
        exit(-1);
    }

    void* game_dll = sx_os_dlopen(game_filepath);
    if (game_dll == NULL) {
        sx_snprintf(errmsg, sizeof(errmsg),
                    "Game module '%s' is not a valid DLL: %s", game_filepath, sx_os_dlerr());
        rizz__app_message_box(errmsg);
        exit(-1);
    }

    rizz_game_config_cb* game_config_fn = (rizz_game_config_cb*)sx_os_dlsym(game_dll, "rizz_game_config");
    if (!game_config_fn) {
        sx_snprintf(errmsg, sizeof(errmsg), "Symbol 'rizz_game_config' not found in game module: %s", game_filepath);
        rizz__app_message_box(errmsg);
        exit(-1);
    }
#else
    game_filepath = argc > 0 ? argv[0] : "";
    rizz_game_config_cb* game_config_fn = rizz_game_config;
#endif    // RIZZ_BUNDLE

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
                         .app_title = default_title,
                         .plugin_path = "",
                         .cwd = cwd,
                         .app_version = 1000,
                         .app_flags = RIZZ_APP_FLAG_USER_CURSOR,
                         .core_flags = 0,
                         .log_level = RIZZ_LOG_LEVEL_INFO,
                         .window_width = 800,
                         .window_height = 600,
                         .multisample_count = 1,
                         .swap_interval = 1,
                         .texture_filter_min = SG_FILTER_LINEAR_MIPMAP_LINEAR,
                         .texture_filter_mag = SG_FILTER_LINEAR,
                         .texture_first_mip = first_mip,
                         .job_num_threads = -1,    // defaults to num_cores-1
                         .job_max_fibers = 64,
                         .job_stack_size = 1024,
                         .coro_num_init_fibers = 64,
                         .coro_stack_size = 2048,
                         .tmp_mem_max = 10*1024,
                         .profiler_listen_port = 17815,    // default remotery port
                         .profiler_update_interval_ms = 10 };

    if (profile_gpu)
        conf.core_flags |= RIZZ_CORE_FLAG_PROFILE_GPU;
    if (dump_unused_assets)
        conf.core_flags |= RIZZ_CORE_FLAG_DUMP_UNUSED_ASSETS;
    if (crash_dump)
        conf.app_flags |= RIZZ_APP_FLAG_CRASH_DUMP;
#ifdef _DEBUG
    conf.core_flags |= RIZZ_CORE_FLAG_DETECT_LEAKS;
#endif

    // load ini file before game config
    rizz__app_load_ini(CONFIG_FILENAME, &conf);

    // run game config / override config
    game_config_fn(&conf, rizz__app_cmdline_arg);

    #if SX_PLATFORM_WINDOWS
        SetUnhandledExceptionFilter(rizz__app_generate_crash_dump);
    #endif

    // command-line arguments must be registered to this point
    // parse them and save them for future use
    sx_cmdline_context* cmdline_app = rizz__app_parse_cmdline(argc, argv);
    if (show_help) {
        puts("rizz (engine) arguments:");
        rizz__app_show_help(cmdline);
        
        if (cmdline_app && sx_array_count(g_app.cmdline_args) > 1) {
            puts("app arguments:");
            rizz__app_show_help(cmdline_app);
        }

        // write to file
        rizz__app_write_help_tofile("rizz_args.txt", cmdline,
                                    sx_array_count(g_app.cmdline_args) > 1 ? cmdline_app : NULL);
    }

    if (cmdline_app) {
        sx_cmdline_destroy_context(cmdline_app, g_app.alloc);
    }
    sx_array_free(g_app.alloc, g_app.cmdline_args);

    // create .cache directory if it doesn't exist
    if (!conf.cache_path && !sx_os_path_isdir(default_cache_path)) {
        sx_os_mkdir(default_cache_path);
    }

    // Before closing the dll, save the strings into static variables
    rizz__app_save_config_str(default_name, conf.app_name);
    rizz__app_save_config_str(default_title, conf.app_title);
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

    return (sapp_desc) {
        .init_cb = rizz__app_init,
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
        .ios_keyboard_resizes_canvas = (conf.app_flags & RIZZ_APP_FLAG_IOS_KEYBOARD_RESIZES_CANVAS) ? true : false,
        .user_cursor = (conf.app_flags & RIZZ_APP_FLAG_USER_CURSOR) ? true : false,
        .gl_force_gles2 = (conf.app_flags & RIZZ_APP_FLAG_FORCE_GLES2) ? true : false,
        .enable_clipboard = (!RIZZ_FINAL && SX_PLATFORM_PC) ? true : false
    };
}

static void rizz__app_window_size(sx_vec2* size)
{
    sx_assert(size);
    *size = g_app.window_size;
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

static const rizz_config* rizz__app_config(void)
{
    return &g_app.conf;
}

static const char* rizz__app_config_meta_value(const char* section, const char* name)
{
    if (g_app.conf_ini) {
        int section_id = sx_ini_find_section(g_app.conf_ini, section, 0);
        if (section_id != -1) {
            int prop_id = sx_ini_find_property(g_app.conf_ini, section_id, name, 0);
            if (prop_id != -1) {
                return sx_ini_property_value(g_app.conf_ini, section_id, prop_id);
            }
        }
    }
    return NULL;
}

static const char* rizz__app_cmdline_arg_value(const char* name)
{
    for (int i = 0; i < sx_array_count(g_app.cmdline_items); i++) {
        rizz__cmdline_item* item = &g_app.cmdline_items[i];
        if (sx_strequal(item->name, name)) {
            return item->allocated_value ? item->value_ptr : item->value;
        }
    }
    return NULL;
}

static bool rizz__app_cmdline_arg_exists(const char* name)
{
    for (int i = 0; i < sx_array_count(g_app.cmdline_items); i++) {
        rizz__cmdline_item* item = &g_app.cmdline_items[i];
        if (sx_strequal(item->name, name)) {
            return true;
        }
    }
    return false;
}

static void rizz__parse_single_shortcut_key(const char* keystr, rizz_keycode keys[2], int* num_keys, rizz_modifier_keys* mods)
{
    int len = sx_strlen(keystr);
    // function keys
    bool is_fn = (len == 2 || len == 3) && sx_toupperchar(keystr[0]) == 'F' && 
        ((len == 2 && sx_isnumchar(keystr[1])) || (len == 3 && sx_isnumchar(keystr[1]) && sx_isnumchar(keystr[2])));
    if (is_fn && *num_keys < 2) {
        char numstr[3] = {keystr[1], keystr[2], 0};
        int fnum = sx_toint(numstr) - 1;
        if (fnum >= 0 && fnum < 25) {
            keys[(*num_keys)++] = RIZZ_APP_KEYCODE_F1 + fnum;
        }
    }
    else if (len > 1) {
        char modstr[32];
        sx_toupper(modstr, sizeof(modstr), keystr);
        if (sx_strequal(modstr, "ALT")) {
            *mods |= RIZZ_APP_MODIFIERKEY_ALT;
        } else if (sx_strequal(modstr, "CTRL")) {
            *mods |= RIZZ_APP_MODIFIERKEY_CTRL;
        } else if (sx_strequal(modstr, "SHIFT")) {
            *mods |= RIZZ_APP_MODIFIERKEY_SHIFT;
        }
    } 
    else if (len == 1 && *num_keys < 2) {
        char key = sx_toupperchar(keystr[0]);
        if (keystr[0] > RIZZ_APP_KEYCODE_SPACE) {
            keys[(*num_keys)++] = (rizz_keycode)key;
        }
    }
}

// shortcut string example:
// "key1+key2+mod1+mod2+.."
// "K+SHIFT+CTRL"
static void rizz__parse_shortcut_string(const char* shortcut, rizz_keycode keys[2], rizz_modifier_keys* mods)
{
    shortcut = sx_skip_whitespace(shortcut);

    int num_keys = 0;
    const char* plus;
    char keystr[32];

    while (shortcut[0]) {
        plus = sx_strchar(shortcut, '+');
        if (!plus) {
            break;
        }
        sx_strncpy(keystr, sizeof(keystr), shortcut, (int)(uintptr_t)(plus - shortcut));
        rizz__parse_single_shortcut_key(keystr, keys, &num_keys, mods);
        shortcut = sx_skip_whitespace(plus + 1);
    }

    // read the last one
    if (shortcut[0]) {
        sx_strcpy(keystr, sizeof(keystr), shortcut);
        rizz__parse_single_shortcut_key(keystr, keys, &num_keys, mods);
    }
}

static void rizz__register_shortcut(const char* shortcut, rizz_app_shortcut_cb* shortcut_cb, void* user)
{
    sx_assert(shortcut_cb);

    rizz_keycode keys[2] = {0, 0};
    rizz_modifier_keys mods = 0;

    rizz__parse_shortcut_string(shortcut, keys, &mods);
    sx_assertf(keys[0], "Invalid shortcut string (example: A+CTRL)");
    if (keys[0]) {
        rizz__shortcut_item item = {
            .callback = shortcut_cb,
            .keys[0] = keys[0],
            .keys[1] = keys[1],
            .mods = mods,
            .user = user
        };
        sx_array_push(g_app.alloc, g_app.shortcuts, item);
    } 
}

static void rizz__set_crash_callback(void (*crash_cb)(void*, void*), void* user)
{
    g_app.crash_cb = crash_cb;
    g_app.crash_user_data = user;
}

rizz_api_app the__app = { .width = sapp_width,
                          .height = sapp_height,
                          .window_size = rizz__app_window_size,
                          .highdpi = sapp_high_dpi,
                          .dpiscale = sapp_dpi_scale,
                          .config = rizz__app_config,
                          .config_meta_value = rizz__app_config_meta_value,
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
                          .mouse_release = rizz__app_mouse_release,
                          .cmdline_arg_value = rizz__app_cmdline_arg_value,
                          .cmdline_arg_exists = rizz__app_cmdline_arg_exists,
                          .set_clipboard_string = sapp_set_clipboard_string,
                          .clipboard_string = sapp_get_clipboard_string,
                          .register_shortcut = rizz__register_shortcut,
                          .set_crash_callback = rizz__set_crash_callback };
