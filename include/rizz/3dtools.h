#pragma once

#include "rizz/rizz.h"

typedef struct rizz_prims3d_vertex {
    sx_vec3 pos;
    sx_vec3 normal;
    sx_vec2 uv;
} rizz_prims3d_vertex;

typedef enum rizz_prims3d_map_type {
    RIZZ_PRIMS3D_MAPTYPE_WHITE = 0,
    RIZZ_PRIMS3D_MAPTYPE_CHECKER
} rizz_prims3d_map_type;

// all generated geometry are in index based triangles (3 indices = tri)
typedef struct rizz_prims3d_geometry {
    rizz_prims3d_vertex* verts;
    uint16_t* indices;
    int num_verts;
    int num_indices;
} rizz_prims3d_geometry;

////////////////////////////////////////////////////////////////////////////////////////////////////
// @prims3d
typedef struct rizz_api_prims3d {
    // boxes are drawn solid. when drawing mutiple boxes, don't mix alpha/solid color tints
    void (*draw_box)(const sx_box* box, const sx_mat4* viewproj_mat, rizz_prims3d_map_type map_type,
                     sx_color tint);
    void (*draw_boxes)(const sx_box* boxes, int num_boxes, const sx_mat4* viewproj_mat,
                       rizz_prims3d_map_type map_type, sx_color* tints);
    bool (*generate_box_geometry)(const sx_alloc* alloc, rizz_prims3d_geometry* geo);

    // aabbs are always wireframe. and alpha in color tint doesn't affect them
    void (*draw_aabb)(const sx_aabb* aabb, sx_color tint);
    void (*draw_aabbs)(const sx_aabb* aabbs, int num_aabbs, sx_color* tints);
} rizz_api_prims3d;

