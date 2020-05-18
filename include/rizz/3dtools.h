#pragma once

#include "rizz/rizz.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// @prims3d
typedef struct rizz_api_prims3d {
    // boxes are drawn solid. when drawing mutiple boxes, don't mix alpha/solid color tints
    void (*draw_box)(const sx_box* box, sx_color tints);
    void (*draw_boxes)(const sx_box* boxes, int num_boxes, sx_color* tints);

    // aabbs are always wireframe. and alpha in color tint doesn't affect them
    void (*draw_aabb)(const sx_aabb* aabb, sx_color tint);
    void (*draw_aabbs)(const sx_aabb* aabbs, int num_aabbs, sx_color* tints);
} rizz_api_prims3d;

