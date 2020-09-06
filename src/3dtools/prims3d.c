#include "rizz/3dtools.h"
#include "rizz/rizz.h"

#include "sx/allocator.h"
#include "sx/linear-buffer.h"

#include "3dtools-internal.h"
#include "sx/math.h"

#define MAX_INSTANCES 1000
#define MAX_DYN_VERTICES 10000
#define MAX_DYN_INDICES 10000

RIZZ_STATE static rizz_api_core* the_core;
RIZZ_STATE static rizz_api_gfx* the_gfx;
RIZZ_STATE static rizz_api_camera* the_camera;

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
    sx_vec3 scale;  
    sx_color color;
} prims3d__instance;

typedef struct prims3d__uniforms {
    sx_mat4 viewproj_mat;
} prims3d__uniforms;

typedef struct prims3d__context {
    const sx_alloc* alloc;
    rizz_api_gfx_draw* draw_api;
    sg_shader shader;
    sg_shader shader_wire;
    sg_pipeline pip_solid;
    sg_pipeline pip_alphablend;
    sg_pipeline pip_wire;
    sg_pipeline pip_wire_strip;
    sg_pipeline pip_wire_ib;
    sg_buffer dyn_vbuff;
    sg_buffer dyn_ibuff;
    sg_buffer instance_buff;
    prims3d__shape unit_box;
    rizz_texture checker_tex;
    int64_t updated_stats_frame;
    int max_instances;
    int max_verts;
    int max_indices;
    int num_instances;
    int num_verts;
    int num_indices;
} prims3d__context;

typedef struct prims3d__instance_depth {
    int index;
    float z;
} prims3d__instance_depth;

#define SORT_NAME prims3d__instance
#define SORT_TYPE prims3d__instance_depth
#define SORT_CMP(x, y) ((y).z - (x).z)
SX_PRAGMA_DIAGNOSTIC_PUSH()
SX_PRAGMA_DIAGNOSTIC_IGNORED_MSVC(4267)
SX_PRAGMA_DIAGNOSTIC_IGNORED_MSVC(4244)
SX_PRAGMA_DIAGNOSTIC_IGNORED_MSVC(4146)
SX_PRAGMA_DIAGNOSTIC_IGNORED_CLANG_GCC("-Wunused-function")
SX_PRAGMA_DIAGNOSTIC_IGNORED_CLANG("-Wshorten-64-to-32")
#include "sort/sort.h"
SX_PRAGMA_DIAGNOSTIC_POP()

static prims3d__context g_prims3d;

static const rizz_vertex_layout k_prims3d_vertex_layout_inst = {
    .attrs[0] = { .semantic = "POSITION", .offset = offsetof(rizz_prims3d_vertex, pos)},
    .attrs[1] = { .semantic = "NORMAL", .offset = offsetof(rizz_prims3d_vertex, normal)},
    .attrs[2] = { .semantic = "TEXCOORD", .offset = offsetof(rizz_prims3d_vertex, uv)},
    .attrs[3] = { .semantic = "TEXCOORD", .offset = offsetof(prims3d__instance, tx1), .semantic_idx=1, .buffer_index=1},
    .attrs[4] = { .semantic = "TEXCOORD", .offset = offsetof(prims3d__instance, tx2), .semantic_idx=2, .buffer_index=1},
    .attrs[5] = { .semantic = "TEXCOORD", .offset = offsetof(prims3d__instance, tx3), .semantic_idx=3, .buffer_index=1},
    .attrs[6] = { .semantic = "TEXCOORD", .offset = offsetof(prims3d__instance, scale), .semantic_idx=4, .buffer_index=1},
    .attrs[7] = { .semantic = "TEXCOORD", .offset = offsetof(prims3d__instance, color), .format = SG_VERTEXFORMAT_UBYTE4N, .semantic_idx=5, .buffer_index=1}
};

static const rizz_vertex_layout k_prims3d_vertex_layout_wire = {
    .attrs[0] = { .semantic = "POSITION", .offset = offsetof(rizz_prims3d_vertex, pos)},
    .attrs[1] = { .semantic = "COLOR", .offset = offsetof(rizz_prims3d_vertex, color), .format = SG_VERTEXFORMAT_UBYTE4N}
};

#include rizz_shader_path(shaders_h, prims3d.vert.h)
#include rizz_shader_path(shaders_h, prims3d.frag.h)

#include rizz_shader_path(shaders_h, wireframe.vert.h)
#include rizz_shader_path(shaders_h, wireframe.frag.h)

bool prims3d__init(rizz_api_core* core, rizz_api_gfx* gfx, rizz_api_camera* cam)
{
    the_core = core;
    the_gfx = gfx;
    the_camera = cam;
    g_prims3d.draw_api = &the_gfx->staged;
    g_prims3d.alloc = the_core->alloc(RIZZ_MEMID_GRAPHICS);
    
    rizz_temp_alloc_begin(tmp_alloc);

    rizz_shader shader_solid = the_gfx->shader_make_with_data(tmp_alloc,
        k_prims3d_vs_size, k_prims3d_vs_data, k_prims3d_vs_refl_size, k_prims3d_vs_refl_data,
        k_prims3d_fs_size, k_prims3d_fs_data, k_prims3d_fs_refl_size, k_prims3d_fs_refl_data);
    rizz_shader shader_wire = the_gfx->shader_make_with_data(tmp_alloc,
        k_wireframe_vs_size, k_wireframe_vs_data, k_wireframe_vs_refl_size, k_wireframe_vs_refl_data,
        k_wireframe_fs_size, k_wireframe_fs_data, k_wireframe_fs_refl_size, k_wireframe_fs_refl_data);

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
    sg_pipeline_desc pip_desc_alphablend = 
        (sg_pipeline_desc){ .layout.buffers[0].stride = sizeof(rizz_prims3d_vertex),
                            .layout.buffers[1].stride = sizeof(prims3d__instance),
                            .layout.buffers[1].step_func = SG_VERTEXSTEP_PER_INSTANCE,
                            .rasterizer = { .cull_mode = SG_CULLMODE_BACK },
                            .index_type = SG_INDEXTYPE_UINT16,
                            .blend = {
                                .enabled = true,
                                .src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA,
                                .dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA
                            },
                            .depth_stencil = {
                                .depth_compare_func = SG_COMPAREFUNC_LESS_EQUAL,
                            } };                            
    sg_pipeline_desc pip_desc_wire = 
        (sg_pipeline_desc) { .layout.buffers[0].stride = sizeof(rizz_prims3d_vertex),
                             .shader = shader_wire.shd,
                             .index_type = SG_INDEXTYPE_NONE,
                             .primitive_type = SG_PRIMITIVETYPE_LINES,
                             .depth_stencil = { .depth_compare_func = SG_COMPAREFUNC_LESS_EQUAL,
                                                .depth_write_enabled = true } };    
    sg_pipeline pip_solid = the_gfx->make_pipeline(
        the_gfx->shader_bindto_pipeline(&shader_solid, &pip_desc_solid, &k_prims3d_vertex_layout_inst));
    sg_pipeline pip_alphablend = the_gfx->make_pipeline(
        the_gfx->shader_bindto_pipeline(&shader_solid, &pip_desc_alphablend, &k_prims3d_vertex_layout_inst));
    sg_pipeline pip_wire = the_gfx->make_pipeline(
        the_gfx->shader_bindto_pipeline(&shader_wire, &pip_desc_wire, &k_prims3d_vertex_layout_wire));

    pip_desc_wire.primitive_type = SG_PRIMITIVETYPE_LINE_STRIP;
    sg_pipeline pip_wire_strip = the_gfx->make_pipeline(
        the_gfx->shader_bindto_pipeline(&shader_wire, &pip_desc_wire, &k_prims3d_vertex_layout_wire));

    pip_desc_wire.index_type = SG_INDEXTYPE_UINT16;
    sg_pipeline pip_wire_ib = the_gfx->make_pipeline(
        the_gfx->shader_bindto_pipeline(&shader_wire, &pip_desc_wire, &k_prims3d_vertex_layout_wire));

    if (pip_solid.id == 0 || shader_wire.shd.id == 0 || pip_wire.id == 0 || shader_solid.shd.id == 0 || 
        pip_alphablend.id == 0 || pip_wire_ib.id == 0) {
        return false;
    }

    g_prims3d.shader = shader_solid.shd;
    g_prims3d.shader_wire = shader_wire.shd;
    g_prims3d.pip_solid = pip_solid;
    g_prims3d.pip_alphablend = pip_alphablend;
    g_prims3d.pip_wire = pip_wire;
    g_prims3d.pip_wire_ib = pip_wire_ib;
    g_prims3d.pip_wire_strip = pip_wire_strip;

    g_prims3d.dyn_vbuff = the_gfx->make_buffer(&(sg_buffer_desc){
        .type = SG_BUFFERTYPE_VERTEXBUFFER,
        .usage = SG_USAGE_STREAM,
        .size = sizeof(rizz_prims3d_vertex) * MAX_DYN_VERTICES });
    g_prims3d.dyn_ibuff = the_gfx->make_buffer(&(sg_buffer_desc){
        .type = SG_BUFFERTYPE_INDEXBUFFER,
        .usage = SG_USAGE_STREAM,
        .size = sizeof(rizz_prims3d_vertex) * MAX_DYN_INDICES });
    g_prims3d.instance_buff = the_gfx->make_buffer(&(sg_buffer_desc) {
        .size = sizeof(prims3d__instance) * MAX_INSTANCES,
        .type = SG_BUFFERTYPE_VERTEXBUFFER,
        .usage = SG_USAGE_STREAM,
    });
    if (g_prims3d.dyn_vbuff.id == 0 || g_prims3d.dyn_ibuff.id == 0 || g_prims3d.instance_buff.id == 0) {
        return false;
    }

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

    // checker texture 
    sx_color checker_colors[] = {
        sx_color4u(200, 200, 200, 255), 
        sx_color4u(255, 255, 255, 255)
    };
    g_prims3d.checker_tex = the_gfx->texture_create_checker(64, 128, checker_colors);

    g_prims3d.max_instances = MAX_INSTANCES;
    g_prims3d.max_verts = MAX_DYN_VERTICES;
    g_prims3d.max_indices = MAX_DYN_INDICES;

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

    if (g_prims3d.checker_tex.img.id) {
        the_gfx->destroy_image(g_prims3d.checker_tex.img);
    }
    if (g_prims3d.pip_solid.id) {
        the_gfx->destroy_pipeline(g_prims3d.pip_solid);
    }
    if (g_prims3d.pip_alphablend.id) {
        the_gfx->destroy_pipeline(g_prims3d.pip_alphablend);
    }
    if (g_prims3d.pip_wire.id) {
        the_gfx->destroy_pipeline(g_prims3d.pip_wire);
    }
    if (g_prims3d.pip_wire_strip.id) {
        the_gfx->destroy_pipeline(g_prims3d.pip_wire_strip);
    }
    if (g_prims3d.pip_wire_ib.id) {
        the_gfx->destroy_pipeline(g_prims3d.pip_wire_ib);
    }
    if (g_prims3d.instance_buff.id) {
        the_gfx->destroy_buffer(g_prims3d.instance_buff);
    }
    if (g_prims3d.dyn_vbuff.id) {
        the_gfx->destroy_buffer(g_prims3d.dyn_vbuff);
    }
    if (g_prims3d.dyn_ibuff.id) {
        the_gfx->destroy_buffer(g_prims3d.dyn_ibuff);
    }
    if (g_prims3d.shader.id) {
        the_gfx->destroy_shader(g_prims3d.shader);
    }
}

static inline void prims3d__reset_stats(void)
{
    int64_t frame = the_core->frame_index();
    if (frame != g_prims3d.updated_stats_frame) {
        g_prims3d.updated_stats_frame = frame;
        g_prims3d.num_indices = g_prims3d.num_verts = g_prims3d.num_instances = 0;
    }
}

void prims3d__draw_box(const sx_box* box, const sx_mat4* viewproj_mat,
                       rizz_prims3d_map_type map_type, sx_color tint)
{
    prims3d__draw_boxes(box, 1, viewproj_mat, map_type, &tint);
}

void prims3d__draw_boxes(const sx_box* boxes, int num_boxes, const sx_mat4* viewproj_mat,
                         rizz_prims3d_map_type map_type, const sx_color* tints)
{
    sx_assert(num_boxes > 0);

    rizz_api_gfx_draw* draw_api = g_prims3d.draw_api;

    prims3d__uniforms uniforms = {
        .viewproj_mat = *viewproj_mat
    };

    // clang-format off
    sg_image map;
    switch (map_type) {
    case RIZZ_PRIMS3D_MAPTYPE_WHITE:        map = the_gfx->texture_white(); break;
    case RIZZ_PRIMS3D_MAPTYPE_CHECKER:      map = g_prims3d.checker_tex.img; break;
    }
    // clang-format on

    rizz_temp_alloc_begin(tmp_alloc);

    prims3d__reset_stats();
    sx_assert_rel((g_prims3d.num_instances + num_boxes) <= g_prims3d.max_instances);
    prims3d__instance* instances = sx_malloc(tmp_alloc, sizeof(prims3d__instance)*num_boxes);
    if (!instances) {
        sx_out_of_memory();
        return;
    }

    uint8_t first_alpha = 255;
    if (tints) {
        first_alpha = tints[0].a;
    }

    for (int i = 0; i < num_boxes; i++) {
        const sx_tx3d* tx = &boxes[i].tx;
        prims3d__instance* instance = &instances[i];
        instance->tx1 = sx_vec4f(tx->pos.x, tx->pos.y, tx->pos.z, tx->rot.m11);
        instance->tx2 = sx_vec4f(tx->rot.m21, tx->rot.m31, tx->rot.m12, tx->rot.m22);
        instance->tx3 = sx_vec4f(tx->rot.m23, tx->rot.m13, tx->rot.m23, tx->rot.m33);
        instance->scale = sx_vec3_mulf(boxes[i].e, 2.0f);
        instance->color = tints ? tints[i] : SX_COLOR_WHITE;
        sx_assert(!tints || (first_alpha == tints[i].a));
    }

    // in alpha-blend mode (transparent boxes), sort the boxes from back-to-front
    if (first_alpha != 255) {
        prims3d__instance_depth* sort_items = sx_malloc(tmp_alloc, sizeof(prims3d__instance_depth)*num_boxes);
        if (!sort_items) {
            sx_out_of_memory();
            return;
        }
        for (int i = 0; i < num_boxes; i++) {
            sort_items[i].index = i;
            sort_items[i].z = sx_mat4_mul_vec3(viewproj_mat, boxes[i].tx.pos).z;
        }

        prims3d__instance_tim_sort(sort_items, num_boxes);

        prims3d__instance* sorted = sx_malloc(tmp_alloc, sizeof(prims3d__instance)*num_boxes);
        if (!sorted) {
            sx_out_of_memory();
            return;
        }
        for (int i = 0; i < num_boxes; i++) {
            sorted[i] = instances[sort_items[i].index];
        }
        instances = sorted;
    }

    int inst_offset = draw_api->append_buffer(g_prims3d.instance_buff, instances,
                                              sizeof(prims3d__instance) * num_boxes);
    g_prims3d.num_instances += num_boxes;

    draw_api->apply_pipeline(first_alpha == 255 ? g_prims3d.pip_solid : g_prims3d.pip_alphablend);
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
    sx_aabb a = sx_aabbf(-e, -e, -e, e, e, e);
    sx_vec3 corners[8];
    sx_aabb_corners(corners, &a);
    
    // X+
    verts[0] = (rizz_prims3d_vertex) { .pos = corners[1], .normal = sx_vec3f(1.0f, 0, 0), .uv = sx_vec2f(0, 1.0f), .color = sx_colorn(0xffffffff) };
    verts[1] = (rizz_prims3d_vertex) { .pos = corners[3], .normal = sx_vec3f(1.0f, 0, 0), .uv = sx_vec2f(0, 0), .color = sx_colorn(0xffffffff) };
    verts[2] = (rizz_prims3d_vertex) { .pos = corners[7], .normal = sx_vec3f(1.0f, 0, 0), .uv = sx_vec2f(1.0f, 0), .color = sx_colorn(0xffffffff) };
    verts[3] = (rizz_prims3d_vertex) { .pos = corners[5], .normal = sx_vec3f(1.0f, 0, 0), .uv = sx_vec2f(1.0f, 1.0f), .color = sx_colorn(0xffffffff) };
    indices[0] = 0;         indices[1] = 1;         indices[2] = 3;
    indices[3] = 1;         indices[4] = 2;         indices[5] = 3;

    // X-
    verts[4] = (rizz_prims3d_vertex) { .pos = corners[4], .normal = sx_vec3f(-1.0f, 0, 0), .uv = sx_vec2f(0, 1.0f), .color = sx_colorn(0xffffffff) };
    verts[5] = (rizz_prims3d_vertex) { .pos = corners[6], .normal = sx_vec3f(-1.0f, 0, 0), .uv = sx_vec2f(0, 0), .color = sx_colorn(0xffffffff) };
    verts[6] = (rizz_prims3d_vertex) { .pos = corners[2], .normal = sx_vec3f(-1.0f, 0, 0), .uv = sx_vec2f(1.0f, 0), .color = sx_colorn(0xffffffff) };
    verts[7] = (rizz_prims3d_vertex) { .pos = corners[0], .normal = sx_vec3f(-1.0f, 0, 0), .uv = sx_vec2f(1.0f, 1.0f), .color = sx_colorn(0xffffffff) };
    indices[6] = 4;         indices[7] = 5;         indices[8] = 7;
    indices[9] = 5;         indices[10] = 6;        indices[11] = 7;

    // Y-
    verts[8] = (rizz_prims3d_vertex) { .pos = corners[0], .normal = sx_vec3f(0, -1.0f, 0), .uv = sx_vec2f(0, 1.0f), .color = sx_colorn(0xffffffff) };
    verts[9] = (rizz_prims3d_vertex) { .pos = corners[2], .normal = sx_vec3f(0, -1.0f, 0), .uv = sx_vec2f(0, 0), .color = sx_colorn(0xffffffff) };
    verts[10] = (rizz_prims3d_vertex) { .pos = corners[3], .normal = sx_vec3f(0, -1.0f, 0), .uv = sx_vec2f(1.0f, 0), .color = sx_colorn(0xffffffff) };
    verts[11] = (rizz_prims3d_vertex) { .pos = corners[1], .normal = sx_vec3f(0, -1.0f, 0), .uv = sx_vec2f(1.0f, 1.0f), .color = sx_colorn(0xffffffff) };
    indices[12] = 8;       indices[13] = 9;        indices[14] = 11;
    indices[15] = 9;       indices[16] = 10;       indices[17] = 11;

    // Y+
    verts[12] = (rizz_prims3d_vertex) { .pos = corners[5], .normal = sx_vec3f(0, 1.0f, 0), .uv = sx_vec2f(0, 1.0f), .color = sx_colorn(0xffffffff) };
    verts[13] = (rizz_prims3d_vertex) { .pos = corners[7], .normal = sx_vec3f(0, 1.0f, 0), .uv = sx_vec2f(0, 0), .color = sx_colorn(0xffffffff) };
    verts[14] = (rizz_prims3d_vertex) { .pos = corners[6], .normal = sx_vec3f(0, 1.0f, 0), .uv = sx_vec2f(1.0f, 0), .color = sx_colorn(0xffffffff) };
    verts[15] = (rizz_prims3d_vertex) { .pos = corners[4], .normal = sx_vec3f(0, 1.0f, 0), .uv = sx_vec2f(1.0f, 1.0f), .color = sx_colorn(0xffffffff) };
    indices[18] = 12;      indices[19] = 13;      indices[20] = 15;
    indices[21] = 13;      indices[22] = 14;      indices[23] = 15;

    // Z-
    verts[16] = (rizz_prims3d_vertex) { .pos = corners[1], .normal = sx_vec3f(0, 0, -1.0f), .uv = sx_vec2f(0, 1.0f), .color = sx_colorn(0xffffffff) };
    verts[17] = (rizz_prims3d_vertex) { .pos = corners[5], .normal = sx_vec3f(0, 0, -1.0f), .uv = sx_vec2f(0, 0), .color = sx_colorn(0xffffffff) };
    verts[18] = (rizz_prims3d_vertex) { .pos = corners[4], .normal = sx_vec3f(0, 0, -1.0f), .uv = sx_vec2f(1.0f, 0), .color = sx_colorn(0xffffffff) };
    verts[19] = (rizz_prims3d_vertex) { .pos = corners[0], .normal = sx_vec3f(0, 0, -1.0f), .uv = sx_vec2f(1.0f, 1.0f), .color = sx_colorn(0xffffffff) };
    indices[24] = 16;      indices[25] = 17;      indices[26] = 19;
    indices[27] = 17;      indices[28] = 18;      indices[29] = 19;

    // Z+
    verts[20] = (rizz_prims3d_vertex) { .pos = corners[2], .normal = sx_vec3f(0, 0, 1.0f), .uv = sx_vec2f(0, 1.0f), .color = sx_colorn(0xffffffff) };
    verts[21] = (rizz_prims3d_vertex) { .pos = corners[6], .normal = sx_vec3f(0, 0, 1.0f), .uv = sx_vec2f(0, 0), .color = sx_colorn(0xffffffff) };
    verts[22] = (rizz_prims3d_vertex) { .pos = corners[7], .normal = sx_vec3f(0, 0, 1.0f), .uv = sx_vec2f(1.0f, 0), .color = sx_colorn(0xffffffff) };
    verts[23] = (rizz_prims3d_vertex) { .pos = corners[3], .normal = sx_vec3f(0, 0, 1.0f), .uv = sx_vec2f(1.0f, 1.0f), .color = sx_colorn(0xffffffff) };
    indices[30] = 20;      indices[31] = 21;      indices[32] = 23;
    indices[33] = 21;      indices[34] = 22;      indices[35] = 23;    
    // clang-format on

    return true;
}

void prims3d__free_geometry(rizz_prims3d_geometry* geo, const sx_alloc* alloc)
{
    sx_assert(geo);
    sx_free(alloc, geo->verts);
}

void prims3d__draw_aabb(const sx_aabb* aabb, const sx_mat4* viewproj_mat, sx_color tint)
{
    prims3d__draw_aabbs(aabb, 1, viewproj_mat, &tint);
}

void prims3d__draw_aabbs(const sx_aabb* aabbs, int num_aabbs, const sx_mat4* viewproj_mat, const sx_color* tints)
{
    sx_assert(aabbs);
    sx_assert(num_aabbs > 0);
    
    const int num_verts = 8 * num_aabbs;
    const int num_indices = 24 * num_aabbs;
    rizz_api_gfx_draw* draw_api = g_prims3d.draw_api;
    sx_vec3 corners[8];

    prims3d__reset_stats();
    sx_assert_rel((g_prims3d.num_verts + num_verts) <= g_prims3d.max_verts);
    sx_assert_rel((g_prims3d.num_verts + num_indices) <= g_prims3d.max_indices);

    rizz_temp_alloc_begin(tmp_alloc);

    size_t total_sz = num_verts*sizeof(rizz_prims3d_vertex) + num_indices*sizeof(uint16_t);
    uint8_t* buff = sx_malloc(tmp_alloc, total_sz);
    if (!buff) {
        sx_out_of_memory();
        return;
    }

    rizz_prims3d_vertex* vertices = (rizz_prims3d_vertex*)buff;
    buff += sizeof(rizz_prims3d_vertex)*num_verts;
    uint16_t* indices = (uint16_t*)buff;

    rizz_prims3d_vertex* _verts = vertices;
    uint16_t* _indices = indices;
    for (int i = 0; i < num_aabbs; i++) {
        sx_aabb_corners(corners, &aabbs[i]);
        for (int vi = 0; vi < 8; vi++) {
            _verts[vi].pos = corners[vi];
            _verts[vi].color = tints ? tints[i] : SX_COLOR_WHITE;
        }
        _verts += 8;
        
        int vindex = i*8;
        _indices[0] = vindex;             _indices[1] = vindex + 1;           
        _indices[2] = vindex + 1;         _indices[3] = vindex + 3;
        _indices[4] = vindex + 3;         _indices[5] = vindex + 2;
        _indices[6] = vindex + 2;         _indices[7] = vindex + 0;
        _indices[8] = vindex + 5;         _indices[9] = vindex + 4;
        _indices[10] = vindex + 4;        _indices[11] = vindex + 6;
        _indices[12] = vindex + 6;        _indices[13] = vindex + 7;
        _indices[14] = vindex + 7;        _indices[15] = vindex + 5;
        _indices[16] = vindex + 5;        _indices[17] = vindex + 1;
        _indices[18] = vindex + 7;        _indices[19] = vindex + 3;
        _indices[20] = vindex + 0;        _indices[21] = vindex + 4;
        _indices[22] = vindex + 2;        _indices[23] = vindex + 6;

        _indices += 24;
    }

    int vb_offset = draw_api->append_buffer(g_prims3d.dyn_vbuff, vertices, num_verts * sizeof(rizz_prims3d_vertex));
    int ib_offset = draw_api->append_buffer(g_prims3d.dyn_ibuff, indices, num_indices * sizeof(uint16_t));
    g_prims3d.num_verts += num_verts;
    g_prims3d.num_indices += num_indices;

    sg_bindings bind = { 
        .vertex_buffers[0] = g_prims3d.dyn_vbuff, 
        .index_buffer = g_prims3d.dyn_ibuff,
        .vertex_buffer_offsets[0] = vb_offset,
        .index_buffer_offset = ib_offset };

    draw_api->apply_pipeline(g_prims3d.pip_wire_ib);
    draw_api->apply_uniforms(SG_SHADERSTAGE_VS, 0, viewproj_mat, sizeof(*viewproj_mat));
    draw_api->apply_bindings(&bind);
    draw_api->draw(0, num_indices, 1);

    rizz_temp_alloc_end(tmp_alloc);
}

void prims3d__grid_xzplane(float spacing, float spacing_bold, const sx_mat4* vp, const sx_vec3 frustum[8])
{
    static const sx_color color = { { 170, 170, 170, 255 } };
    static const sx_color bold_color = { { 255, 255, 255, 255 } };

    rizz_api_gfx_draw* draw_api = g_prims3d.draw_api;

    spacing = sx_ceil(sx_max(spacing, 0.0001f));
    sx_aabb bb = sx_aabb_empty();

    // extrude near plane
    sx_vec3 near_plane_norm = sx_plane_normal(frustum[0], frustum[1], frustum[2]);
    for (int i = 0; i < 8; i++) {
        if (i < 4) {
            sx_vec3 offset_pt = sx_vec3_sub(frustum[i], sx_vec3_mulf(near_plane_norm, spacing));
            sx_aabb_add_point(&bb, sx_vec3f(offset_pt.x, 0, offset_pt.z));
        } else {
            sx_aabb_add_point(&bb, sx_vec3f(frustum[i].x, 0, frustum[i].z));
        }
    }

    // snap grid bounds to `spacing`
    int nspace = (int)spacing;
    sx_aabb snapbox = sx_aabbf((float)((int)bb.xmin - (int)bb.xmin % nspace), 0,
                               (float)((int)bb.zmin - (int)bb.zmin % nspace),
                               (float)((int)bb.xmax - (int)bb.xmax % nspace), 0,
                               (float)((int)bb.zmax - (int)bb.zmax % nspace));
    float w = snapbox.xmax - snapbox.xmin;
    float d = snapbox.zmax - snapbox.zmin;
    if (sx_equal(w, 0, 0.00001f) || sx_equal(d, 0, 0.00001f))
        return;

    int xlines = (int)w / nspace + 1;
    int ylines = (int)d / nspace + 1;
    int num_verts = (xlines + ylines) * 2;

    prims3d__reset_stats();
    sx_assert_rel((g_prims3d.num_verts + num_verts) <= g_prims3d.max_verts);

    // draw
    int data_size = num_verts * sizeof(rizz_prims3d_vertex);
    rizz_temp_alloc_begin(tmp_alloc);
    rizz_prims3d_vertex* verts = sx_malloc(tmp_alloc, data_size);
    sx_assert(verts);

    int i = 0;
    for (float zoffset = snapbox.zmin; zoffset <= snapbox.zmax; zoffset += spacing, i += 2) {
        verts[i].pos.x = snapbox.xmin;
        verts[i].pos.y = 0;
        verts[i].pos.z = zoffset;

        int ni = i + 1;
        verts[ni].pos.x = snapbox.xmax;
        verts[ni].pos.y = 0;
        verts[ni].pos.z = zoffset;

        verts[i].color = verts[ni].color =
            (zoffset != 0.0f)
                ? (!sx_equal(sx_mod(zoffset, spacing_bold), 0.0f, 0.0001f) ? color : bold_color)
                : SX_COLOR_RED;
    }

    for (float xoffset = snapbox.xmin; xoffset <= snapbox.xmax; xoffset += spacing, i += 2) {
        verts[i].pos.x = xoffset;
        verts[i].pos.y = 0;
        verts[i].pos.z = snapbox.zmin;

        int ni = i + 1;
        sx_assert(ni < num_verts);
        verts[ni].pos.x = xoffset;
        verts[ni].pos.y = 0;
        verts[ni].pos.z = snapbox.zmax;

        verts[i].color = verts[ni].color =
            (xoffset != 0.0f)
                ? (!sx_equal(sx_mod(xoffset, spacing_bold), 0.0f, 0.0001f) ? color : bold_color)
                : SX_COLOR_BLUE;
    }

    int offset = draw_api->append_buffer(g_prims3d.dyn_vbuff, verts, data_size);
    g_prims3d.num_verts += num_verts;

    sg_bindings bind = { 
        .vertex_buffers[0] = g_prims3d.dyn_vbuff, 
        .vertex_buffer_offsets[0] = offset };

    draw_api->apply_pipeline(g_prims3d.pip_wire);
    draw_api->apply_uniforms(SG_SHADERSTAGE_VS, 0, vp, sizeof(*vp));
    draw_api->apply_bindings(&bind);
    draw_api->draw(0, num_verts, 1);

    rizz_temp_alloc_end(tmp_alloc);
}

void prims3d__grid_xyplane(float spacing, float spacing_bold, const sx_mat4* vp, const sx_vec3 frustum[8])
{
    static const sx_color color = { { 170, 170, 170, 255 } };
    static const sx_color bold_color = { { 255, 255, 255, 255 } };

    rizz_api_gfx_draw* draw_api = g_prims3d.draw_api;

    spacing = sx_ceil(sx_max(spacing, 0.0001f));
    sx_aabb bb = sx_aabb_empty();

    // extrude near plane
    sx_vec3 near_plane_norm = sx_plane_normal(frustum[0], frustum[1], frustum[2]);
    for (int i = 0; i < 8; i++) {
        if (i < 4) {
            sx_vec3 offset_pt = sx_vec3_sub(frustum[i], sx_vec3_mulf(near_plane_norm, spacing));
            sx_aabb_add_point(&bb, sx_vec3f(offset_pt.x, offset_pt.y, 0));
        } else {
            sx_aabb_add_point(&bb, sx_vec3f(frustum[i].x, frustum[i].y, 0));
        }
    }

    // snap grid bounds to `spacing`
    int nspace = (int)spacing;
    sx_aabb snapbox = sx_aabbf((float)((int)bb.xmin - (int)bb.xmin % nspace),
                               (float)((int)bb.ymin - (int)bb.ymin % nspace), 0,
                               (float)((int)bb.xmax - (int)bb.xmax % nspace),
                               (float)((int)bb.ymax - (int)bb.ymax % nspace), 0);
    float w = snapbox.xmax - snapbox.xmin;
    float h = snapbox.ymax - snapbox.ymin;
    if (sx_equal(w, 0, 0.00001f) || sx_equal(h, 0, 0.00001f))
        return;

    int xlines = (int)w / nspace + 1;
    int ylines = (int)h / nspace + 1;
    int num_verts = (xlines + ylines) * 2;

    prims3d__reset_stats();
    sx_assert_rel((g_prims3d.num_verts + num_verts) <= g_prims3d.max_verts);

    // draw
    int data_size = num_verts * sizeof(rizz_prims3d_vertex);
    rizz_temp_alloc_begin(tmp_alloc);
    rizz_prims3d_vertex* verts = sx_malloc(tmp_alloc, data_size);
    sx_assert(verts);

    int i = 0;
    for (float yoffset = snapbox.ymin; yoffset <= snapbox.ymax; yoffset += spacing, i += 2) {
        verts[i].pos.x = snapbox.xmin;
        verts[i].pos.y = yoffset;
        verts[i].pos.z = 0;

        int ni = i + 1;
        verts[ni].pos.x = snapbox.xmax;
        verts[ni].pos.y = yoffset;
        verts[ni].pos.z = 0;

        verts[i].color = verts[ni].color =
            (yoffset != 0.0f)
                ? (!sx_equal(sx_mod(yoffset, spacing_bold), 0.0f, 0.0001f) ? color : bold_color)
                : SX_COLOR_RED;
    }

    for (float xoffset = snapbox.xmin; xoffset <= snapbox.xmax; xoffset += spacing, i += 2) {
        verts[i].pos.x = xoffset;
        verts[i].pos.y = snapbox.ymin;
        verts[i].pos.z = 0;

        int ni = i + 1;
        sx_assert(ni < num_verts);
        verts[ni].pos.x = xoffset;
        verts[ni].pos.y = snapbox.ymax;
        verts[ni].pos.z = 0;

        verts[i].color = verts[ni].color =
            (xoffset != 0.0f)
                ? (!sx_equal(sx_mod(xoffset, spacing_bold), 0.0f, 0.0001f) ? color : bold_color)
                : SX_COLOR_GREEN;
    }

    int offset = draw_api->append_buffer(g_prims3d.dyn_vbuff, verts, data_size);
    g_prims3d.num_verts += num_verts;

    sg_bindings bind = { 
        .vertex_buffers[0] = g_prims3d.dyn_vbuff, 
        .vertex_buffer_offsets[0] = offset };

    draw_api->apply_pipeline(g_prims3d.pip_wire);
    draw_api->apply_uniforms(SG_SHADERSTAGE_VS, 0, vp, sizeof(*vp));
    draw_api->apply_bindings(&bind);
    draw_api->draw(0, num_verts, 1);
    rizz_temp_alloc_end(tmp_alloc);
}

void prims3d__grid_xyplane_cam(float spacing, float spacing_bold, float dist, const rizz_camera* cam, 
                               const sx_mat4* viewproj_mat)
{
    sx_vec3 frustum[8];
    the_camera->calc_frustum_points_range(cam, frustum, -dist, dist);
    prims3d__grid_xyplane(spacing, spacing_bold, viewproj_mat, frustum);
}

void prims3d__set_max_instances(int max_instances)
{
    sx_assert(max_instances > 0);

    if (g_prims3d.instance_buff.id) {
        the_gfx->destroy_buffer(g_prims3d.instance_buff);
    }

    g_prims3d.instance_buff = the_gfx->make_buffer(&(sg_buffer_desc) {
        .size = sizeof(prims3d__instance) * max_instances,
        .type = SG_BUFFERTYPE_VERTEXBUFFER,
        .usage = SG_USAGE_STREAM,
    });

    sx_assert_rel(g_prims3d.instance_buff.id);
    g_prims3d.max_instances = max_instances;
}

void prims3d__set_max_vertices(int max_verts)
{
    sx_assert(max_verts > 0);

    if (g_prims3d.dyn_vbuff.id) {
        the_gfx->destroy_buffer(g_prims3d.dyn_vbuff);
    }

    g_prims3d.dyn_vbuff = the_gfx->make_buffer(&(sg_buffer_desc){
        .type = SG_BUFFERTYPE_VERTEXBUFFER,
        .usage = SG_USAGE_STREAM,
        .size = sizeof(rizz_prims3d_vertex) * max_verts });

    sx_assert_rel(g_prims3d.instance_buff.id);
    g_prims3d.max_verts = max_verts;
}

void prims3d__set_max_indices(int max_indices)
{
    sx_assert(max_indices > 0);

    if (g_prims3d.dyn_ibuff.id) {
        the_gfx->destroy_buffer(g_prims3d.dyn_ibuff);
    }

    g_prims3d.dyn_ibuff = the_gfx->make_buffer(&(sg_buffer_desc){
        .type = SG_BUFFERTYPE_INDEXBUFFER,
        .usage = SG_USAGE_STREAM,
        .size = sizeof(rizz_prims3d_vertex) * max_indices });

    sx_assert_rel(g_prims3d.dyn_ibuff.id);
    g_prims3d.max_indices = max_indices;
}

void prims3d__draw_path(const sx_vec3* points, int num_points, const sx_mat4* viewproj_mat, const sx_color color)
{
    int const num_verts = num_points;
    int const data_size = num_verts * sizeof(rizz_prims3d_vertex);
    rizz_api_gfx_draw* draw_api = g_prims3d.draw_api;

    prims3d__reset_stats();
    sx_assert_rel((g_prims3d.num_verts + num_verts) <= g_prims3d.max_verts);

    rizz_temp_alloc_begin(tmp_alloc);
    rizz_prims3d_vertex* verts = sx_malloc(tmp_alloc, data_size);

    for (int i = 0; i < num_points; i++) {
        verts[i].pos = points[i];
        verts[i].color = color;
    }

    int offset = draw_api->append_buffer(g_prims3d.dyn_vbuff, verts, data_size);
    g_prims3d.num_verts += num_verts;
    sg_bindings bind = { 
        .vertex_buffers[0] = g_prims3d.dyn_vbuff, 
        .vertex_buffer_offsets[0] = offset };

    draw_api->apply_pipeline(g_prims3d.pip_wire_strip);
    draw_api->apply_uniforms(SG_SHADERSTAGE_VS, 0, viewproj_mat, sizeof(*viewproj_mat));
    draw_api->apply_bindings(&bind);
    draw_api->draw(0, num_verts, 1);

    rizz_temp_alloc_end(tmp_alloc);
}

void prims3d__draw_line(const sx_vec3 p0, const sx_vec3 p1, const sx_mat4* viewproj_mat, const sx_color color)
{
    int const num_verts = 2;
    int const data_size = num_verts * sizeof(rizz_prims3d_vertex);
    rizz_api_gfx_draw* draw_api = g_prims3d.draw_api;

    prims3d__reset_stats();
    sx_assert_rel((g_prims3d.num_verts + num_verts) <= g_prims3d.max_verts);

    rizz_temp_alloc_begin(tmp_alloc);
    rizz_prims3d_vertex* verts = sx_malloc(tmp_alloc, data_size);

    verts[0].pos = p0;
    verts[0].color = color;

    verts[1].pos = p1;
    verts[1].color = color;

    int offset = draw_api->append_buffer(g_prims3d.dyn_vbuff, verts, data_size);
    g_prims3d.num_verts += num_verts;
    sg_bindings bind = { 
        .vertex_buffers[0] = g_prims3d.dyn_vbuff, 
        .vertex_buffer_offsets[0] = offset };

    draw_api->apply_pipeline(g_prims3d.pip_wire);
    draw_api->apply_uniforms(SG_SHADERSTAGE_VS, 0, viewproj_mat, sizeof(*viewproj_mat));
    draw_api->apply_bindings(&bind);
    draw_api->draw(0, num_verts, 1);

    rizz_temp_alloc_end(tmp_alloc);
}
