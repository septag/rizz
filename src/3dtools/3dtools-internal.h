#pragma once

#include "sx/math.h"
#include "rizz/3dtools.h"

bool prims3d__init(rizz_api_core* core, rizz_api_gfx* gfx);
void prims3d__release(void);

void prims3d__draw_box(const sx_box* box, const sx_mat4* viewproj_mat, rizz_prims3d_map_type map_type,
                     sx_color tint);
void prims3d__draw_boxes(const sx_box* boxes, int num_boxes, const sx_mat4* viewproj_mat,
                         rizz_prims3d_map_type map_type, const sx_color* tints);
bool prims3d__generate_box_geometry(const sx_alloc* alloc, rizz_prims3d_geometry* geo);

void prims3d__draw_aabb(const sx_aabb* aabb, sx_color tint);
void prims3d__draw_aabbs(const sx_aabb* aabbs, int num_aabbs, const sx_color* tints);
