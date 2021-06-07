#pragma once

#include "sx/os.h"
#include "rizz/imgui.h"
#include "rizz/rizz.h"

#define EXAMPLES_DEFAULT_WIDTH  1280
#define EXAMPLES_DEFAULT_HEIGHT 800

static inline char* ex_shader_path(char* path, int path_sz, const char* prefix_path, const char* filename) {
    sx_os_path_join(path, path_sz, prefix_path, sx_stringize(RIZZ_GRAPHICS_SHADER_LANG));
    sx_os_path_join(path, path_sz, path, filename);
    return sx_os_path_unixpath(path, path_sz, path);
}


static inline void show_debugmenu(rizz_api_imgui* imgui, rizz_api_core* core)
{
    static bool show_memory_debugger = false;
    static bool show_log = false;
    static bool show_graphics = false;

    if (imgui->BeginMainMenuBar()) {
        if (imgui->BeginMenu("Debug", true)) {
            if (imgui->MenuItem_Bool("Memory", NULL, show_memory_debugger, true)) {
                show_memory_debugger = !show_memory_debugger;
            }
            if (imgui->MenuItem_Bool("Log", NULL, show_log, true)) {
                show_log = !show_log;
            }
            if (imgui->MenuItem_Bool("Graphics", NULL, show_graphics, true)) {
                show_graphics = !show_graphics;
            }
            imgui->EndMenu();
        }
    }
    imgui->EndMainMenuBar();

    if (show_memory_debugger) {
        core->show_memory_debugger(&show_memory_debugger);
    }

    if (show_log) {
        core->show_log(&show_log);
    }

    if (show_graphics) {
        core->show_graphics_debugger(&show_graphics);
    }
}