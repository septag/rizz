#pragma once

#include "sx/math.h"

typedef struct rizz_prims3d_geometry rizz_prims3d_geometry;
typedef struct sx_alloc sx_alloc;
typedef struct rizz_api_core rizz_api_core;
typedef struct rizz_api_gfx rizz_api_gfx;

bool prims3d__init(rizz_api_core* core, rizz_api_gfx* gfx);
void prims3d__release(void);

void prims3d__draw_box(const sx_box* box, const sx_mat4* viewproj_mat, sx_color tint);
void prims3d__draw_boxes(const sx_box* boxes, int num_boxes, const sx_mat4* viewproj_mat, sx_color* tints);
bool prims3d__generate_box_geometry(const sx_alloc* alloc, rizz_prims3d_geometry* geo);

void prims3d__draw_aabb(const sx_aabb* aabb, sx_color tint);
void prims3d__draw_aabbs(const sx_aabb* aabbs, int num_aabbs, sx_color* tints);
