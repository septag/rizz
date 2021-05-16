#pragma once

#include "sx/math-types.h"
#include "rizz/3dtools.h"

typedef rizz_camera rizz_camera;
typedef struct rizz_api_imgui rizz_api_imgui;

bool debug3d__init(rizz_api_core* core, rizz_api_gfx* gfx, rizz_api_camera* cam);
void debug3d__release(void);

void debug3d__draw_box(const sx_box* box, const sx_mat4* viewproj_mat, rizz_3d_debug_map_type map_type,
                     sx_color tint);
void debug3d__draw_boxes(const sx_box* boxes, int num_boxes, const sx_mat4* viewproj_mat,
                         rizz_3d_debug_map_type map_type, const sx_color* tints);
void debug3d__draw_sphere(sx_vec3 center, float radius, const sx_mat4* viewproj_mat,
                          rizz_3d_debug_map_type map_type, sx_color tint);
void debug3d__draw_spheres(const sx_vec3* centers, const float* radiuss, int count, 
                           const sx_mat4* viewproj_mat, rizz_3d_debug_map_type map_type, 
                           const sx_color* tints);
void debug3d__draw_cones(const float* radiuss, const float* depths, const sx_tx3d* txs, int count, 
                         const sx_mat4* viewproj_mat, const sx_color* tints);
void debug3d__draw_cone(float radius, float depth, const sx_tx3d* tx, const sx_mat4* viewproj_mat, sx_color tint);

bool debug3d__generate_box_geometry(const sx_alloc* alloc, rizz_3d_debug_geometry* geo, sx_vec3 extents);
bool debug3d__generate_sphere_geometry(const sx_alloc* alloc, rizz_3d_debug_geometry* geo,
                                       float radius, int num_segments, int num_rings);
bool debug3d__generate_cone_geometry(const sx_alloc* alloc, rizz_3d_debug_geometry* geo,
                                     int num_segments, float radius1, float radius2, float depth);
void debug3d__free_geometry(rizz_3d_debug_geometry* geo, const sx_alloc* alloc);
void debug3d__grid_xzplane(float spacing, float spacing_bold, const sx_mat4* vp, const sx_vec3 frustum[8]);
void debug3d__grid_xyplane(float spacing, float spacing_bold, const sx_mat4* vp, const sx_vec3 frustum[8]);
void debug3d__grid_xyplane_cam(float spacing, float spacing_bold, float dist, const rizz_camera* cam, 
                               const sx_mat4* viewproj_mat);

void debug3d__draw_aabb(const sx_aabb* aabb, const sx_mat4* viewproj_mat, sx_color tint);
void debug3d__draw_aabbs(const sx_aabb* aabbs, int num_aabbs, const sx_mat4* viewproj_mat, const sx_color* tints);

void debug3d__draw_path(const sx_vec3* points, int num_points, const sx_mat4* viewproj_mat, const sx_color color);
void debug3d__draw_line(const sx_vec3 p0, const sx_vec3 p1, const sx_mat4* viewproj_mat, const sx_color color);
void debug3d__draw_lines(int num_lines, const rizz_3d_debug_line* lines, const sx_mat4* viewproj_mat, const sx_color* colors);
void debug3d__draw_axis(const sx_mat4* mat, const sx_mat4* viewproj_mat, float scale);
void debug3d__draw_camera(const rizz_camera* cam, const sx_mat4* viewproj_mat);

void debug3d__set_draw_api(rizz_api_gfx_draw* draw_api);
void debug3d__set_max_instances(int max_instances);
void debug3d__set_max_vertices(int max_verts);
void debug3d__set_max_indices(int max_indices);

bool model__init(rizz_api_core* core, rizz_api_asset* asset, rizz_api_gfx* gfx, rizz_api_imgui* imgui);
void model__release(void);
void model__set_imgui(rizz_api_imgui* imgui);
const rizz_model* model__get(rizz_asset model_asset);
void model__set_material_lib(rizz_material_lib* mtllib);

rizz_material material__add(rizz_material_lib* lib, const rizz_material_data* mtldata);
void material__remove(rizz_material_lib* lib, rizz_material mtl);
rizz_material_lib* material__create_lib(const sx_alloc* alloc, int init_capacity);
void material__destroy_lib(rizz_material_lib* lib);
rizz_material material__get_blank(const rizz_material_lib* lib);
