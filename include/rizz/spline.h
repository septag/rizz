#pragma once

#include "rizz/rizz.h"

#define RIZZ_SPLINE3D_NODE_FIELDS \
    sx_vec3 pos;                  \
    sx_vec3 lwing;                \
    sx_vec3 rwing;

#define RIZZ_SPLINE2D_NODE_FIELDS \
    sx_vec2 pos;                  \
    sx_vec2 lwing;                \
    sx_vec2 rwing;

typedef struct rizz_spline3d_node {
    RIZZ_SPLINE3D_NODE_FIELDS
} rizz_spline3d_node;

typedef struct rizz_spline2d_node {
    RIZZ_SPLINE2D_NODE_FIELDS
} rizz_spline2d_node;

typedef struct rizz_spline2d_desc {
    const rizz_spline2d_node* nodes;
    uint32_t num_nodes;
    uint32_t node_stride;
    float time;
    void (*usereval)(const rizz_spline2d_node* n1, const rizz_spline2d_node* n2, float t,
                     sx_vec2* result);
    bool norm, loop;
} rizz_spline2d_desc;

typedef struct rizz_spline3d_desc {
    const rizz_spline3d_node* nodes;
    uint32_t num_nodes;
    uint32_t node_stride;
    float time;
    void (*usereval)(const rizz_spline3d_node* n1, const rizz_spline3d_node* n2, float t,
                     sx_vec3* result);
    bool norm, loop;
} rizz_spline3d_desc;

typedef struct rizz_api_spline {
    void (*eval2d)(const rizz_spline2d_desc* desc, sx_vec2* result);
    void (*eval3d)(const rizz_spline3d_desc* desc, sx_vec3* result);
} rizz_api_spline;