#pragma once

#include "rizz/rizz.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// @prims3d
typedef struct rizz_prims3d_vertex {
    sx_vec3 pos;
    sx_vec3 normal;
    sx_vec2 uv;
    sx_color color;
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

typedef struct rizz_api_prims3d {
    // boxes are drawn solid. when drawing mutiple boxes, don't mix alpha/solid color tints
    void (*draw_box)(const sx_box* box, const sx_mat4* viewproj_mat, rizz_prims3d_map_type map_type,
                     sx_color tint);
    void (*draw_boxes)(const sx_box* boxes, int num_boxes, const sx_mat4* viewproj_mat,
                       rizz_prims3d_map_type map_type, const sx_color* tints);
    bool (*generate_box_geometry)(const sx_alloc* alloc, rizz_prims3d_geometry* geo);
    void (*free_geometry)(rizz_prims3d_geometry* geo, const sx_alloc* alloc);

    // aabbs are always wireframe. and alpha in color tint doesn't affect them
    void (*draw_aabb)(const sx_aabb* aabb, const sx_mat4* viewproj_mat, sx_color tint);
    void (*draw_aabbs)(const sx_aabb* aabbs, int num_aabbs, const sx_mat4* viewproj_mat, const sx_color* tints);

    void (*grid_xzplane)(float spacing, float spacing_bold, const sx_mat4* viewproj_mat, const sx_vec3 frustum[8]);
    void (*grid_xyplane)(float spacing, float spacing_bold, const sx_mat4* viewproj_mat, const sx_vec3 frustum[8]);
    void (*grid_xyplane_cam)(float spacing, float spacing_bold, float dist, const rizz_camera* cam, 
                             const sx_mat4* viewproj_mat);    
} rizz_api_prims3d;

////////////////////////////////////////////////////////////////////////////////////////////////////
// @material
typedef struct rizz_material_texture {
    rizz_asset tex_asset;
    int array_index;
} rizz_material_texture;

typedef struct rizz_material_metallic_roughness {
    rizz_material_texture base_color_tex;
    rizz_material_texture metallic_roughness_tex;
    
    sx_vec4 base_color_factor;
    float metallic_factor;
    float roughness_factor;
} rizz_material_metallic_roughness;

typedef struct rizz_material_specular_glossiness {
    rizz_material_texture diffuse_tex;
    rizz_material_texture specular_glossiness_tex;

    sx_vec4 diffuse_factor;
    sx_vec3 specular_factor;
    float glossiness_factor;
} rizz_material_specular_glossiness;

typedef struct rizz_meterial_clearcoat {
    rizz_asset clearcoat_tex;
    rizz_asset clearcoat_roughness_tex;
    rizz_asset clearcoat_normal_tex;

    float clearcoat_factor;
    float clearcoat_roughness_factor;
} rizz_material_clearcoat;

typedef enum rizz_material_alpha_mode {
    RIZZ_MATERIAL_ALPHAMODE_OPAQUE = 0,
    RIZZ_MATERIAL_ALPHAMODE_MASK,
    RIZZ_MATERIAL_ALPHAMODE_BLEND
} rizz_material_alpha_mode;

typedef struct rizz_material_data {
    char name[32];
    bool has_metal_roughness;
    bool has_specular_glossiness;
    bool has_clearcoat;
    bool _reserved1;
    rizz_material_metallic_roughness pbr_metallic_roughness;
    rizz_material_specular_glossiness pbr_specular_glossiness;
    rizz_material_clearcoat clearcoat;
    rizz_material_texture normal_tex;
    rizz_material_texture occlusion_tex;
    rizz_material_texture emissive_tex;
    sx_vec4 emissive_factor;
    rizz_material_alpha_mode alpha_mode;
    float alpha_cutoff;
    bool double_sided;
    bool unlit;
} rizz_material_data;

typedef struct { uint32_t id; } rizz_material;


////////////////////////////////////////////////////////////////////////////////////////////////////
// @model
// mesh contains the main geometry for each node of the model
// it includes multiple "submeshes". a submesh is part of the geometry with differnt material
// so, a mesh can contain multiple materials within itself
// vertex buffers are another topic that needs to be discussed here:
//      when loading a model, you should define the whole vertex-layout that your render pipeline uses
//      so for example, a renderer may need to layout it's vertex data like this:
//          - buffer #1: position
//          - buffer #2: normal/tangent
//          - buffer #3: texcoord
//          - buffer #4: joints/weights
//      4 buffers will be reserved (num_vbuffs=4) for every model loaded with this pipeline setup
//        - if the source model doesn't have joints AND weights. then the buffer #4 slot for the model will be NULL
//        - if the source model does have normal but does not have tangents, then buffer #2 will be created and tangents will be undefined
//      When rendering, you can customize which set of buffers you'll need based on the shader's input layout
//        - shadow map shader: can only fetch the buffer #1 (position)
//      The catch is when you setup your pipeline, all shaders should comply for one or more vertex-buffer formats
//      So in our example, every shader must take one of the 4 buffer formats or multiple of them
// vertex_attrs is all vertex attributes of the source model and is not related for vertex buffer formats
// gpu_t struct is created only for models without STREAM buffer flag
//
// mandate:
//  - instance-able
//  - optionally store gpu buffers for immediate rendering
//  - joinable (texture-arrays + )
//  - editable, including instances (dynamic/stream buffers)
//  - joints and skeleton
//  - skinning
//  - wireframe (bc coords)
//  - compression (quantize)
//  - multi-stream (base/skin/tangent/extra)
//      - gbuffer: pos+normal+color+skin+tangent+etc..
//      - gbuffer (def-light): position+normal+tangent
//      - shadows: position (+skin)
//      - light: texcoord + normal
//  - instancing optimization (some kind of hash?)
//  - we can pass a vertex layout and model data will be matched by that ! 
//      - debugging: we need to see all instances of a mesh in the scene
//

// see above comments to more description on this 
typedef struct rizz_model_geometry_layout {
    rizz_vertex_attr attrs[SG_MAX_VERTEX_ATTRIBUTES];
    int buffer_strides[SG_MAX_SHADERSTAGE_BUFFERS];
} rizz_model_geometry_layout;

// provide this for loading "model" asset
// if layout is zero initialized, default layout will be used (same as rizz_prims3d_vertex):
//      buffer #1: position/normal/uv/color
//      if you, leave ibuff_usage/vbuff_usage = default (=0), no gpu buffers will be created
typedef struct rizz_model_load_params {
    rizz_model_geometry_layout layout;
    sg_usage vbuff_usage;
    sg_usage ibuff_usage;
} rizz_model_load_params;

typedef struct rizz_model_submesh {
    int start_index;
    int num_indices;
    rizz_material mtl;
} rizz_model_submesh;

typedef struct rizz_model_mesh {
    char name[32];
    int num_submeshes;
    int num_vertices;
    int num_indices;
    int num_vbuffs;
    sg_index_type index_type;
    rizz_model_submesh* submeshes;

    struct cpu_t {
        void* vbuffs[SG_MAX_SHADERSTAGE_BUFFERS];      // arbitary struct for each vbuff (count=num_vbuffs)
        void* ibuff;                                   // data type is based on index_type
    } cpu;

    struct gpu_t {
        sg_buffer vbuffs[SG_MAX_SHADERSTAGE_BUFFERS];
        sg_buffer ibuff;
    } gpu;
} rizz_model_mesh;

typedef struct rizz_model_node {
    char name[32];
    int mesh_id;        // =-1 if it's not renderable
    int parent_id;      // index to rizz_model.nodes
    int num_childs;     
    sx_tx3d local_tx;
    sx_aabb bounds;
    int* children;      // indices to rizz_model.nodes
} rizz_model_node;

// rizz_model will be stored inside rizz_asset handle 
typedef struct rizz_model {
    int num_meshes;
    int num_nodes;

    sx_tx3d root_tx;

    rizz_model_node* nodes;
    rizz_model_mesh* meshes;
    rizz_model_geometry_layout layout;
} rizz_model;

typedef struct rizz_api_model {
    const rizz_model* (*model_get)(rizz_asset model_asset);
    const rizz_material_data* (*material_get)(rizz_material mtl);
} rizz_api_model;

