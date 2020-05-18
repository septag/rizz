#pragma once

#include "sx/math.h"

void prims3d__draw_box(const sx_box* box, sx_color tints);
void prims3d__draw_boxes(const sx_box* boxes, int num_boxes, sx_color* tints);

void prims3d__draw_aabb(const sx_aabb* aabb, sx_color tint);
void prims3d__draw_aabbs(const sx_aabb* aabbs, int num_aabbs, sx_color* tints);