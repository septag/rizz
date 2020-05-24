#include "rizz/3dtools.h"
#include "rizz/rizz.h"

#include "sx/allocator.h"
#include "sx/linear-buffer.h"

#include "3dtools-internal.h"
#include "sx/math.h"

#define MAX_INSTANCES 1000

RIZZ_STATE static rizz_api_core* the_core;
RIZZ_STATE static rizz_api_gfx* the_gfx;

typedef struct prims3d__shape {
    sg_buffer vb;
    sg_buffer ib;
    int num_verts;
    int num_indices;
} prims3d__shape;

typedef struct prims3d__instance {
    sx_vec4 tx1;    // pos=(x, y, z) , rot(m11)
    sx_vec4 tx2;    // rot(m21, m31, m12, m22)
    sx_vec4 tx3;    // rot(m23, m13, m23, m33)
    sx_vec3 scale;  // scale
} prims3d__instance;

typedef struct prims3d__uniforms {
    sx_mat4 viewproj_mat;
    sx_vec4 color;
} prims3d__uniforms;

typedef struct prims3d__context {
    const sx_alloc* alloc;
    rizz_api_gfx_draw* draw_api;
    sg_shader shader;
    sg_pipeline pip_solid;
    sg_pipeline pip_alphablend;
    sg_buffer instance_buff;
    prims3d__shape unit_box;
} prims3d__context;

static prims3d__context g_prims3d;

static const rizz_vertex_layout k_prims3d_vertex_layout = {
    .attrs[0] = { .semantic = "POSITION", .offset = offsetof(rizz_prims3d_vertex, pos)},
    .attrs[1] = { .semantic = "NORMAL", .offset = offsetof(rizz_prims3d_vertex, normal)},
    .attrs[2] = { .semantic = "TEXCOORD", .offset = offsetof(rizz_prims3d_vertex, uv)},
    .attrs[3] = { .semantic = "TEXCOORD", .offset = offsetof(prims3d__instance, tx1), .semantic_idx=1, .buffer_index=1},
    .attrs[4] = { .semantic = "TEXCOORD", .offset = offsetof(prims3d__instance, tx2), .semantic_idx=2, .buffer_index=1},
    .attrs[5] = { .semantic = "TEXCOORD", .offset = offsetof(prims3d__instance, tx3), .semantic_idx=3, .buffer_index=1},
    .attrs[6] = { .semantic = "TEXCOORD", .offset = offsetof(prims3d__instance, scale), .semantic_idx=4, .buffer_index=1}
};

#include rizz_shader_path(shaders_h, prims3d.vert.h)
#include rizz_shader_path(shaders_h, prims3d.frag.h)

bool prims3d__init(rizz_api_core* core, rizz_api_gfx* gfx)
{
    the_core = core;
    the_gfx = gfx;
    g_prims3d.draw_api = &the_gfx->staged;
    
    rizz_temp_alloc_begin(tmp_alloc);
    g_prims3d.alloc = the_core->alloc(RIZZ_MEMID_GRAPHICS);

    rizz_shader shader_solid = the_gfx->shader_make_with_data(tmp_alloc,
        k_prims3d_vs_size, k_prims3d_vs_data, k_prims3d_vs_refl_size, k_prims3d_vs_refl_data,
        k_prims3d_fs_size, k_prims3d_fs_data, k_prims3d_fs_refl_size, k_prims3d_fs_refl_data);
    sg_pipeline_desc pip_desc_solid = 
        (sg_pipeline_desc){ .layout.buffers[0].stride = sizeof(rizz_prims3d_vertex),
                            .layout.buffers[1].stride = sizeof(prims3d__instance),
                            .layout.buffers[1].step_func = SG_VERTEXSTEP_PER_INSTANCE,
                            .rasterizer = { .cull_mode = SG_CULLMODE_BACK },
                            .index_type = SG_INDEXTYPE_UINT16,
                            .depth_stencil = {
                                .depth_compare_func = SG_COMPAREFUNC_LESS_EQUAL,
                                .depth_write_enabled = true
                            } };
    
    sg_pipeline pip_solid = the_gfx->make_pipeline(
        the_gfx->shader_bindto_pipeline(&shader_solid, &pip_desc_solid, &k_prims3d_vertex_layout));

    if (pip_solid.id == 0 || shader_solid.shd.id == 0) {
        return false;
    }

    g_prims3d.shader = shader_solid.shd;
    g_prims3d.pip_solid = pip_solid;

    g_prims3d.instance_buff = the_gfx->make_buffer(&(sg_buffer_desc) {
        .size = sizeof(prims3d__instance) * MAX_INSTANCES,
        .type = SG_BUFFERTYPE_VERTEXBUFFER,
        .usage = SG_USAGE_STREAM,
    });

    // unit shapes 
    rizz_prims3d_geometry unit_box;
    bool r = prims3d__generate_box_geometry(tmp_alloc, &unit_box);
    sx_unused(r);   sx_assert(r);
    g_prims3d.unit_box.vb = the_gfx->make_buffer(&(sg_buffer_desc) {
        .size = unit_box.num_verts * sizeof(rizz_prims3d_vertex),
        .type = SG_BUFFERTYPE_VERTEXBUFFER,
        .content = unit_box.verts
    });
    g_prims3d.unit_box.ib = the_gfx->make_buffer(&(sg_buffer_desc) {
        .size = unit_box.num_indices * sizeof(uint16_t),
        .type = SG_BUFFERTYPE_INDEXBUFFER,
        .content = unit_box.indices
    });
    g_prims3d.unit_box.num_verts = unit_box.num_verts;
    g_prims3d.unit_box.num_indices = unit_box.num_indices;

    rizz_temp_alloc_end(tmp_alloc);
    return true;
}

static void prims3d__destroy_shape(prims3d__shape* shape) 
{
    if (shape->vb.id) {
        the_gfx->destroy_buffer(shape->vb);
    }
    if (shape->ib.id) {
        the_gfx->destroy_buffer(shape->ib);
    }
}

void prims3d__release(void)
{
    prims3d__destroy_shape(&g_prims3d.unit_box);

    if (g_prims3d.pip_solid.id) {
        the_gfx->destroy_pipeline(g_prims3d.pip_solid);
    }
    if (g_prims3d.shader.id) {
        the_gfx->destroy_shader(g_prims3d.shader);
    }
}

void prims3d__draw_box(const sx_box* box, const sx_mat4* viewproj_mat,
                       rizz_prims3d_map_type map_type, sx_color tint)
{
    rizz_api_gfx_draw* draw_api = g_prims3d.draw_api;

    prims3d__uniforms uniforms = {
        .viewproj_mat = *viewproj_mat,
        .color = sx_color_vec4(tint)
    };

    // clang-format off
    sg_image map;
    switch (map_type) {
    case RIZZ_PRIMS3D_MAPTYPE_WHITE:        map = the_gfx->texture_white(); break;
    case RIZZ_PRIMS3D_MAPTYPE_CHECKER:      map = the_gfx->texture_checker(); break;
    }
    // clang-format on

    prims3d__instance instance;
    const sx_tx3d* tx = &box->tx;
    instance.tx1 = sx_vec4f(tx->pos.x, tx->pos.y, tx->pos.z, tx->rot.m11);
    instance.tx2 = sx_vec4f(tx->rot.m21, tx->rot.m31, tx->rot.m12, tx->rot.m22);
    instance.tx3 = sx_vec4f(tx->rot.m23, tx->rot.m13, tx->rot.m23, tx->rot.m33);
    instance.scale = sx_box_extents(box);

    int inst_offset = draw_api->append_buffer(g_prims3d.instance_buff, &instance, sizeof(instance));

    draw_api->apply_pipeline(g_prims3d.pip_solid);
    draw_api->apply_uniforms(SG_SHADERSTAGE_VS, 0, &uniforms, sizeof(uniforms));
    draw_api->apply_bindings(&(sg_bindings) {
        .vertex_buffers[0] = g_prims3d.unit_box.vb,
        .vertex_buffers[1] = g_prims3d.instance_buff,
        .vertex_buffer_offsets[1] = inst_offset,
        .index_buffer = g_prims3d.unit_box.ib,
        .fs_images[0] = map
    });
    draw_api->draw(0, g_prims3d.unit_box.num_indices, 1);
}

void prims3d__draw_boxes(const sx_box* boxes, int num_boxes, const sx_mat4* viewproj_mat,
                         rizz_prims3d_map_type map_type, sx_color* tints)
{
    sx_assert(num_boxes > 0);

    rizz_api_gfx_draw* draw_api = g_prims3d.draw_api;

    prims3d__uniforms uniforms = {
        .viewproj_mat = *viewproj_mat,
        .color = sx_vec4f(0, 0, 0, 0)
    };

    // clang-format off
    sg_image map;
    switch (map_type) {
    case RIZZ_PRIMS3D_MAPTYPE_WHITE:        map = the_gfx->texture_white(); break;
    case RIZZ_PRIMS3D_MAPTYPE_CHECKER:      map = the_gfx->texture_checker(); break;
    }
    // clang-format on

    rizz_temp_alloc_begin(tmp_alloc);

    prims3d__instance* instances = sx_malloc(tmp_alloc, sizeof(prims3d__instance)*num_boxes);
    if (!instances) {
        sx_out_of_memory();
        return;
    }

    for (int i = 0; i < num_boxes; i++) {
        const sx_tx3d* tx = &boxes[i].tx;
        prims3d__instance* instance = &instances[i];
        instance->tx1 = sx_vec4f(tx->pos.x, tx->pos.y, tx->pos.z, tx->rot.m11);
        instance->tx2 = sx_vec4f(tx->rot.m21, tx->rot.m31, tx->rot.m12, tx->rot.m22);
        instance->tx3 = sx_vec4f(tx->rot.m23, tx->rot.m13, tx->rot.m23, tx->rot.m33);
        instance->scale = sx_box_extents(&boxes[i]);
    }

    int inst_offset = draw_api->append_buffer(g_prims3d.instance_buff, instances,
                                              sizeof(prims3d__instance) * num_boxes);

    draw_api->apply_pipeline(g_prims3d.pip_solid);
    draw_api->apply_uniforms(SG_SHADERSTAGE_VS, 0, &uniforms, sizeof(uniforms));
    draw_api->apply_bindings(&(sg_bindings) {
        .vertex_buffers[0] = g_prims3d.unit_box.vb,
        .vertex_buffers[1] = g_prims3d.instance_buff,
        .vertex_buffer_offsets[1] = inst_offset,
        .index_buffer = g_prims3d.unit_box.ib,
        .fs_images[0] = map
    });
    draw_api->draw(0, g_prims3d.unit_box.num_indices, num_boxes);

    rizz_temp_alloc_end(tmp_alloc);
}

bool prims3d__generate_box_geometry(const sx_alloc* alloc, rizz_prims3d_geometry* geo)
{
    const int num_verts = 24;
    const int num_indices = 36;
    size_t total_sz = num_verts*sizeof(rizz_prims3d_vertex) * num_indices*sizeof(uint16_t);
    uint8_t* buff = sx_malloc(alloc, total_sz);
    if (!buff) {
        sx_out_of_memory();
        return false;
    }

    geo->verts = (rizz_prims3d_vertex*)buff;
    buff += num_verts*sizeof(rizz_prims3d_vertex);
    geo->indices = (uint16_t*)buff;
    geo->num_verts = num_verts;
    geo->num_indices = num_indices;

    // coordinate system: right-handed Z-up
    // winding: CW
    rizz_prims3d_vertex* verts = geo->verts;
    uint16_t* indices = geo->indices;
    const float e = 0.5f;

    // clang-format off
    /*
    *        6                 7
    *        ------------------
    *       /|               /|
    *      / |              / |
    *     /  |             /  |
    *  2 /   |          3 /   |
    *   /----------------/    |
    *   |    |           |    |
    *   |    |           |    |      +Z
    *   |    |           |    |
    *   |    |-----------|----|     |
    *   |   / 4          |   / 5    |  / +Y
    *   |  /             |  /       | /
    *   | /              | /        |/
    *   |/               |/         --------- +X
    *   ------------------
    *  0                 1
    */    

    sx_aabb a = sx_aabbf(-e, -e, -e, e, e, e);

    // X+
    verts[0] = (rizz_prims3d_vertex) { .pos = sx_aabb_corner(&a, 1), .normal = sx_vec3f(1.0f, 0, 0), .uv = sx_vec2f(0, 1.0f) };
    verts[1] = (rizz_prims3d_vertex) { .pos = sx_aabb_corner(&a, 3), .normal = sx_vec3f(1.0f, 0, 0), .uv = sx_vec2f(0, 0) };
    verts[2] = (rizz_prims3d_vertex) { .pos = sx_aabb_corner(&a, 7), .normal = sx_vec3f(1.0f, 0, 0), .uv = sx_vec2f(1.0f, 0) };
    verts[3] = (rizz_prims3d_vertex) { .pos = sx_aabb_corner(&a, 5), .normal = sx_vec3f(1.0f, 0, 0), .uv = sx_vec2f(1.0f, 1.0f) };
    indices[0] = 0;         indices[1] = 1;         indices[2] = 3;
    indices[3] = 1;         indices[4] = 2;         indices[5] = 3;

    // X-
    verts[4] = (rizz_prims3d_vertex) { .pos = sx_aabb_corner(&a, 4), .normal = sx_vec3f(-1.0f, 0, 0), .uv = sx_vec2f(0, 1.0f) };
    verts[5] = (rizz_prims3d_vertex) { .pos = sx_aabb_corner(&a, 6), .normal = sx_vec3f(-1.0f, 0, 0), .uv = sx_vec2f(0, 0) };
    verts[6] = (rizz_prims3d_vertex) { .pos = sx_aabb_corner(&a, 2), .normal = sx_vec3f(-1.0f, 0, 0), .uv = sx_vec2f(1.0f, 0) };
    verts[7] = (rizz_prims3d_vertex) { .pos = sx_aabb_corner(&a, 0), .normal = sx_vec3f(-1.0f, 0, 0), .uv = sx_vec2f(1.0f, 1.0f) };
    indices[6] = 4;         indices[7] = 5;         indices[8] = 7;
    indices[9] = 5;         indices[10] = 6;        indices[11] = 7;

    // Y-
    verts[8] = (rizz_prims3d_vertex) { .pos = sx_aabb_corner(&a, 0), .normal = sx_vec3f(0, -1.0f, 0), .uv = sx_vec2f(0, 1.0f) };
    verts[9] = (rizz_prims3d_vertex) { .pos = sx_aabb_corner(&a, 2), .normal = sx_vec3f(0, -1.0f, 0), .uv = sx_vec2f(0, 0) };
    verts[10] = (rizz_prims3d_vertex) { .pos = sx_aabb_corner(&a, 3), .normal = sx_vec3f(0, -1.0f, 0), .uv = sx_vec2f(1.0f, 0) };
    verts[11] = (rizz_prims3d_vertex) { .pos = sx_aabb_corner(&a, 1), .normal = sx_vec3f(0, -1.0f, 0), .uv = sx_vec2f(1.0f, 1.0f) };
    indices[12] = 8;       indices[13] = 9;        indices[14] = 11;
    indices[15] = 9;       indices[16] = 10;       indices[17] = 11;

    // Y+
    verts[12] = (rizz_prims3d_vertex) { .pos = sx_aabb_corner(&a, 5), .normal = sx_vec3f(0, 1.0f, 0), .uv = sx_vec2f(0, 1.0f) };
    verts[13] = (rizz_prims3d_vertex) { .pos = sx_aabb_corner(&a, 7), .normal = sx_vec3f(0, 1.0f, 0), .uv = sx_vec2f(0, 0) };
    verts[14] = (rizz_prims3d_vertex) { .pos = sx_aabb_corner(&a, 6), .normal = sx_vec3f(0, 1.0f, 0), .uv = sx_vec2f(1.0f, 0) };
    verts[15] = (rizz_prims3d_vertex) { .pos = sx_aabb_corner(&a, 4), .normal = sx_vec3f(0, 1.0f, 0), .uv = sx_vec2f(1.0f, 1.0f) };
    indices[18] = 12;      indices[19] = 13;      indices[20] = 15;
    indices[21] = 13;      indices[22] = 14;      indices[23] = 15;

    // Z-
    verts[16] = (rizz_prims3d_vertex) { .pos = sx_aabb_corner(&a, 1), .normal = sx_vec3f(0, 0, -1.0f), .uv = sx_vec2f(0, 1.0f) };
    verts[17] = (rizz_prims3d_vertex) { .pos = sx_aabb_corner(&a, 5), .normal = sx_vec3f(0, 0, -1.0f), .uv = sx_vec2f(0, 0) };
    verts[18] = (rizz_prims3d_vertex) { .pos = sx_aabb_corner(&a, 4), .normal = sx_vec3f(0, 0, -1.0f), .uv = sx_vec2f(1.0f, 0) };
    verts[19] = (rizz_prims3d_vertex) { .pos = sx_aabb_corner(&a, 0), .normal = sx_vec3f(0, 0, -1.0f), .uv = sx_vec2f(1.0f, 1.0f) };
    indices[24] = 16;      indices[25] = 17;      indices[26] = 19;
    indices[27] = 17;      indices[28] = 18;      indices[29] = 19;

    // Z+
    verts[20] = (rizz_prims3d_vertex) { .pos = sx_aabb_corner(&a, 2), .normal = sx_vec3f(0, 0, 1.0f), .uv = sx_vec2f(0, 1.0f) };
    verts[21] = (rizz_prims3d_vertex) { .pos = sx_aabb_corner(&a, 6), .normal = sx_vec3f(0, 0, 1.0f), .uv = sx_vec2f(0, 0) };
    verts[22] = (rizz_prims3d_vertex) { .pos = sx_aabb_corner(&a, 7), .normal = sx_vec3f(0, 0, 1.0f), .uv = sx_vec2f(1.0f, 0) };
    verts[23] = (rizz_prims3d_vertex) { .pos = sx_aabb_corner(&a, 3), .normal = sx_vec3f(0, 0, 1.0f), .uv = sx_vec2f(1.0f, 1.0f) };
    indices[30] = 20;      indices[31] = 21;      indices[32] = 23;
    indices[33] = 21;      indices[34] = 22;      indices[35] = 23;    
    // clang-format on

    return true;
}

void prims3d__draw_aabb(const sx_aabb* aabb, sx_color tint)
{
    sx_unused(aabb);
    sx_unused(tint);
}

void prims3d__draw_aabbs(const sx_aabb* aabbs, int num_aabbs, sx_color* tints)
{
    sx_unused(aabbs);
    sx_unused(num_aabbs);
    sx_unused(tints);
}
