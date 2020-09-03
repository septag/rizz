#pragma once

#include "sx/math.h"
#include "rizz/3dtools.h"

typedef rizz_camera rizz_camera;
typedef struct rizz_api_imgui rizz_api_imgui;

bool prims3d__init(rizz_api_core* core, rizz_api_gfx* gfx, rizz_api_camera* cam);
void prims3d__release(void);

void prims3d__draw_box(const sx_box* box, const sx_mat4* viewproj_mat, rizz_prims3d_map_type map_type,
                     sx_color tint);
void prims3d__draw_boxes(const sx_box* boxes, int num_boxes, const sx_mat4* viewproj_mat,
                         rizz_prims3d_map_type map_type, const sx_color* tints);
bool prims3d__generate_box_geometry(const sx_alloc* alloc, rizz_prims3d_geometry* geo);
void prims3d__free_geometry(rizz_prims3d_geometry* geo, const sx_alloc* alloc);

void prims3d__grid_xzplane(float spacing, float spacing_bold, const sx_mat4* vp, const sx_vec3 frustum[8]);
void prims3d__grid_xyplane(float spacing, float spacing_bold, const sx_mat4* vp, const sx_vec3 frustum[8]);
void prims3d__grid_xyplane_cam(float spacing, float spacing_bold, float dist, const rizz_camera* cam, 
                               const sx_mat4* viewproj_mat);

void prims3d__draw_aabb(const sx_aabb* aabb, const sx_mat4* viewproj_mat, sx_color tint);
void prims3d__draw_aabbs(const sx_aabb* aabbs, int num_aabbs, const sx_mat4* viewproj_mat, const sx_color* tints);

void prims3d__set_max_instances(int max_instances);
void prims3d__set_max_vertices(int max_verts);
void prims3d__set_max_indices(int max_indices);

bool model__init(rizz_api_core* core, rizz_api_asset* asset, rizz_api_gfx* gfx, rizz_api_imgui* imgui);
void model__release(void);
void model__set_imgui(rizz_api_imgui* imgui);
const rizz_model* model__get(rizz_asset model_asset);
const rizz_material_data* model__get_material(rizz_material mtl);