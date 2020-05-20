#pragma once

#include "sx/math.h"

typedef struct rizz_prims3d_geometry rizz_prims3d_geometry;
typedef struct sx_alloc sx_alloc;

bool prims3d__init(void);
void prims3d__release(void);

void prims3d__draw_box(const sx_box* box, sx_color tints);
void prims3d__draw_boxes(const sx_box* boxes, int num_boxes, sx_color* tints);
bool prims3d__generate_box_geometry(const sx_alloc* alloc, rizz_prims3d_geometry* geo);

void prims3d__draw_aabb(const sx_aabb* aabb, sx_color tint);
void prims3d__draw_aabbs(const sx_aabb* aabbs, int num_aabbs, sx_color* tints);
