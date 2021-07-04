#pragma once

#include "sx/math-types.h"

#include "imgui.h"

typedef struct rizz_mem_info rizz_mem_info;
typedef struct rizz_gfx_trace_info rizz_gfx_trace_info;
typedef struct ImDrawList ImDrawList;

typedef enum { GIZMO_MODE_LOCAL, GIZMO_MODE_WORLD } imgui_gizmo_mode;

// Gizmo
typedef struct rizz_api_imguizmo {
    bool (*hover)();
    bool (*is_using)();
    void (*set_current_window)();
    void (*set_rect)(const sx_rect rc);
    void (*set_ortho)(bool ortho);
    void (*enable)(bool enable);
    void (*decompose_mat4)(const sx_mat4* mat, sx_vec3* translation, sx_vec3* rotation, sx_vec3* scale);
    void (*compose_mat4)(sx_mat4* mat, sx_vec3 translation, sx_vec3 rotation, sx_vec3 scale);
    void (*translate)(sx_mat4* mat, const sx_mat4* view, const sx_mat4* proj, imgui_gizmo_mode mode,
                      sx_mat4* delta_mat, sx_vec3* snap);
    void (*rotation)(sx_mat4* mat, const sx_mat4* view, const sx_mat4* proj, imgui_gizmo_mode mode,
                     sx_mat4* delta_mat, float* snap);
    void (*scale)(sx_mat4* mat, const sx_mat4* view, const sx_mat4* proj, imgui_gizmo_mode mode,
                  sx_mat4* delta_mat, float* snap);
} rizz_api_imguizmo;

typedef struct rizz_api_imgui_extra {
    rizz_api_imguizmo gizmo;

    // these ebuggers are used to monitor inner workings of the engine
    // recommended usage is to use `the_core` API equivalants instead
    void (*graphics_debugger)(const rizz_gfx_trace_info* info, bool* p_open);
    void (*show_log)(bool* p_open);

    void (*dual_progress_bar)(float fraction1, float fraction2, const sx_vec2 ctrl_size,
                              const char* overlay1, const char* overlay2);

    // Full screen 2D drawing
    // You can begin by calling `begin_fullscreen_draw`, fetch and keep the ImDrawList
    // then you can issue ImDrawList_ calls to draw primitives
    ImDrawList* (*begin_fullscreen_draw)(const char* id);

    void (*draw_cursor)(ImDrawList* drawlist, ImGuiMouseCursor cursor, sx_vec2 pos, float scale);

    // use `project_to_screen` and provide mvp matrix to it for to convert from world coords to
    // screen.
    // `viewport` param is optional, if you want to provide a custom vp instead of fullscreen
    sx_vec2 (*project_to_screen)(const sx_vec3 pt, const sx_mat4* mvp, const sx_rect* viewport);

    bool (*is_capturing_mouse)(void);
    bool (*is_capturing_keyboard)(void);

    // returns dock space id (or zero if docking is disabled), 
    // which can be used inside the program to get different docking views
    // particularly useful for drawing inside viewports
    // example:  
    //      ImGuiDockNode* cnode = the_imgui->DockBuilderGetCentralNode(the_imguix->dock_space_id());
    //      SetRenderViewport(cnode->Pos.x, cnode->Pos.y, cnode->Size.x, cnode->Size.y);
    ImGuiID (*dock_space_id)(void);

    void (*label)(const char* name, const char* fmt, ...);
    void (*label_colored)(const ImVec4* name_color, const ImVec4* text_color, const char* name, const char* fmt, ...);
    void (*label_spacing)(float offset, float spacing, const char* name, const char* fmt, ...);
    void (*label_colored_spacing)(const ImVec4* name_color, const ImVec4* text_color, float offset, float spacing, 
                                  const char* name, const char* fmt, ...);
} rizz_api_imgui_extra;