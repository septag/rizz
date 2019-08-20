#pragma once

#include "sx/math.h"

#include "imgui.h"

typedef struct rizz_mem_info rizz_mem_info;
typedef struct rizz_gfx_trace_info rizz_gfx_trace_info;
typedef struct ImDrawList ImDrawList;

typedef enum { GIZMO_MODE_LOCAL, GIZMO_MODE_WORLD } gizmo_mode;

typedef struct rizz_api_imgui_extra {
    void (*memory_debugger)(const rizz_mem_info* info, bool* p_open);
    void (*graphics_debugger)(const rizz_gfx_trace_info* info, bool* p_open);

    // Full screen 2D drawing
    // You can begin by calling `begin_fullscreen_draw`, fetch and keep the ImDrawList
    // then you can issue ImDrawList_ calls to draw primitives
    ImDrawList* (*begin_fullscreen_draw)(const char* id);

    void (*draw_cursor)(ImDrawList* drawlist, ImGuiMouseCursor cursor, sx_vec2 pos, float scale);

    // use `project_to_screen` and provide mvp matrix to it for to convert from world coords to
    // screen.
    // `viewport` param is optional, if you want to provide a custom vp instead of fullscreen
    sx_vec2 (*project_to_screen)(const sx_vec3 pt, const sx_mat4* mvp, const sx_rect* viewport);

    // Guizmo
    bool (*gizmo_hover)();
    bool (*gizmo_using)();
    void (*gizmo_set_window)();
    void (*gizmo_set_rect)(const sx_rect rc);
    void (*gizmo_set_ortho)(bool ortho);
    void (*gizmo_enable)(bool enable);
    void (*gizmo_decompose_mat4)(const sx_mat4* mat, sx_vec3* translation, sx_vec3* rotation,
                                 sx_vec3* scale);
    void (*gizmo_compose_mat4)(sx_mat4* mat, const sx_vec3 translation, const sx_vec3 rotation,
                               const sx_vec3 scale);
    void (*gizmo_translate)(sx_mat4* mat, const sx_mat4* view, const sx_mat4* proj, gizmo_mode mode,
                            sx_mat4* delta_mat, sx_vec3* snap);
    void (*gizmo_rotation)(sx_mat4* mat, const sx_mat4* view, const sx_mat4* proj, gizmo_mode mode,
                           sx_mat4* delta_mat, float* snap);
    void (*gizmo_scale)(sx_mat4* mat, const sx_mat4* view, const sx_mat4* proj, gizmo_mode mode,
                        sx_mat4* delta_mat, float* snap);

} rizz_api_imgui_extra;