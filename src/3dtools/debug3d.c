#include "rizz/3dtools.h"
#include "rizz/rizz.h"

#include "sx/allocator.h"
#include "sx/linear-buffer.h"
#include "sx/math-vec.h"

#include "3dtools-internal.h"

#define MAX_INSTANCES 1000
#define MAX_DYN_VERTICES 10000
#define MAX_DYN_INDICES 10000

RIZZ_STATE static rizz_api_core* the_core;
RIZZ_STATE static rizz_api_gfx* the_gfx;
RIZZ_STATE static rizz_api_camera* the_camera;

typedef struct debug3d__shape {
    sg_buffer vb;
    sg_buffer ib;
    int num_verts;
    int num_indices;
} debug3d__shape;

typedef struct debug3d__instance {
    sx_vec4 tx1;    // pos=(x, y, z) , rot(m11)
    sx_vec4 tx2;    // rot(m21, m31, m12, m22)
    sx_vec4 tx3;    // rot(m23, m13, m23, m33)
    sx_vec3 scale;  
    sx_color color;
} debug3d__instance;

typedef struct debug3d__uniforms {
    sx_mat4 viewproj_mat;
} debug3d__uniforms;

typedef struct debug3d__context {
    const sx_alloc* alloc;
    rizz_api_gfx_draw* draw_api;
    sg_shader shader;
    sg_shader shader_box;
    sg_shader shader_wire;
    sg_pipeline pip_solid;
    sg_pipeline pip_solid_box;
    sg_pipeline pip_alphablend;
    sg_pipeline pip_alphablend_box;
    sg_pipeline pip_wire;
    sg_pipeline pip_wire_strip;
    sg_pipeline pip_wire_ib;
    sg_buffer dyn_vbuff;
    sg_buffer dyn_ibuff;
    sg_buffer instance_buff;
    debug3d__shape unit_box;
    debug3d__shape unit_sphere;
    debug3d__shape unit_cone;
    rizz_texture checker_tex;
    int64_t updated_stats_frame;
    int max_instances;
    int max_verts;
    int max_indices;
    int num_instances;
    int num_verts;
    int num_indices;
} debug3d__context;

typedef struct debug3d__instance_depth {
    int index;
    float z;
} debug3d__instance_depth;

#define SORT_NAME debug3d__instance
#define SORT_TYPE debug3d__instance_depth
#define SORT_CMP(x, y) ((y).z - (x).z)
SX_PRAGMA_DIAGNOSTIC_PUSH()
SX_PRAGMA_DIAGNOSTIC_IGNORED_MSVC(4267)
SX_PRAGMA_DIAGNOSTIC_IGNORED_MSVC(4244)
SX_PRAGMA_DIAGNOSTIC_IGNORED_MSVC(4146)
SX_PRAGMA_DIAGNOSTIC_IGNORED_CLANG_GCC("-Wunused-function")
SX_PRAGMA_DIAGNOSTIC_IGNORED_CLANG("-Wshorten-64-to-32")
#include "sort/sort.h"
SX_PRAGMA_DIAGNOSTIC_POP()

static debug3d__context g_debug3d;

static const rizz_vertex_layout k_prims3d_vertex_layout_inst = {
    .attrs[0] = { .semantic = "POSITION", .offset = offsetof(rizz_3d_debug_vertex, pos)},
    .attrs[1] = { .semantic = "NORMAL", .offset = offsetof(rizz_3d_debug_vertex, normal)},
    .attrs[2] = { .semantic = "TEXCOORD", .offset = offsetof(rizz_3d_debug_vertex, uv)},
    .attrs[3] = { .semantic = "TEXCOORD", .offset = offsetof(debug3d__instance, tx1), .semantic_idx=1, .buffer_index=1},
    .attrs[4] = { .semantic = "TEXCOORD", .offset = offsetof(debug3d__instance, tx2), .semantic_idx=2, .buffer_index=1},
    .attrs[5] = { .semantic = "TEXCOORD", .offset = offsetof(debug3d__instance, tx3), .semantic_idx=3, .buffer_index=1},
    .attrs[6] = { .semantic = "TEXCOORD", .offset = offsetof(debug3d__instance, scale), .semantic_idx=4, .buffer_index=1},
    .attrs[7] = { .semantic = "TEXCOORD", .offset = offsetof(debug3d__instance, color), .format = SG_VERTEXFORMAT_UBYTE4N, .semantic_idx=5, .buffer_index=1}
};

static const rizz_vertex_layout k_prims3d_vertex_layout_wire = {
    .attrs[0] = { .semantic = "POSITION", .offset = offsetof(rizz_3d_debug_vertex, pos)},
    .attrs[1] = { .semantic = "COLOR", .offset = offsetof(rizz_3d_debug_vertex, color), .format = SG_VERTEXFORMAT_UBYTE4N}
};

#include rizz_shader_path(shaders_h, debug3d.vert.h)
#include rizz_shader_path(shaders_h, debug3d.frag.h)

#include rizz_shader_path(shaders_h, debug3d_box.vert.h)
#include rizz_shader_path(shaders_h, debug3d_box.frag.h)

#include rizz_shader_path(shaders_h, debug3d_wire.vert.h)
#include rizz_shader_path(shaders_h, debug3d_wire.frag.h)

bool debug3d__init(rizz_api_core* core, rizz_api_gfx* gfx, rizz_api_camera* cam)
{
    the_core = core;
    the_gfx = gfx;
    the_camera = cam;
    g_debug3d.draw_api = &the_gfx->staged;
    g_debug3d.alloc = the_core->alloc(RIZZ_MEMID_GRAPHICS);
    
    const sx_alloc* tmp_alloc = the_core->tmp_alloc_push();
    sx_scope(the_core->tmp_alloc_pop()) {
        rizz_shader shader_solid = the_gfx->shader_make_with_data(tmp_alloc,
            k_debug3d_vs_size, k_debug3d_vs_data, k_debug3d_vs_refl_size, k_debug3d_vs_refl_data,
            k_debug3d_fs_size, k_debug3d_fs_data, k_debug3d_fs_refl_size, k_debug3d_fs_refl_data);
        rizz_shader shader_solid_box = the_gfx->shader_make_with_data(tmp_alloc,
            k_debug3d_box_vs_size, k_debug3d_box_vs_data, k_debug3d_box_vs_refl_size, k_debug3d_box_vs_refl_data,
            k_debug3d_box_fs_size, k_debug3d_box_fs_data, k_debug3d_box_fs_refl_size, k_debug3d_box_fs_refl_data);        
        rizz_shader shader_wire = the_gfx->shader_make_with_data(tmp_alloc,
            k_debug3d_wire_vs_size, k_debug3d_wire_vs_data, k_debug3d_wire_vs_refl_size, k_debug3d_wire_vs_refl_data,
            k_debug3d_wire_fs_size, k_debug3d_wire_fs_data, k_debug3d_wire_fs_refl_size, k_debug3d_wire_fs_refl_data);

        sg_pipeline_desc pip_desc_solid = 
            (sg_pipeline_desc){ .layout.buffers[0].stride = sizeof(rizz_3d_debug_vertex),
                                .layout.buffers[1].stride = sizeof(debug3d__instance),
                                .layout.buffers[1].step_func = SG_VERTEXSTEP_PER_INSTANCE,
                                .rasterizer = { .cull_mode = SG_CULLMODE_BACK },
                                .index_type = SG_INDEXTYPE_UINT16,
                                .depth_stencil = {
                                    .depth_compare_func = SG_COMPAREFUNC_LESS_EQUAL,
                                    .depth_write_enabled = true
                                },
                                .label = "debug3d_solid"};
        sg_pipeline_desc pip_desc_alphablend = 
            (sg_pipeline_desc){ .layout.buffers[0].stride = sizeof(rizz_3d_debug_vertex),
                                .layout.buffers[1].stride = sizeof(debug3d__instance),
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
                                },
                                .label = "debug3d_alphablend" };
        sg_pipeline_desc pip_desc_wire = 
            (sg_pipeline_desc) { .layout.buffers[0].stride = sizeof(rizz_3d_debug_vertex),
                                 .shader = shader_wire.shd,
                                 .index_type = SG_INDEXTYPE_NONE,
                                 .primitive_type = SG_PRIMITIVETYPE_LINES,
                                 .depth_stencil = { .depth_compare_func = SG_COMPAREFUNC_LESS_EQUAL,
                                                    .depth_write_enabled = true },
                                 .label = "debug3d_wire" };    
        sg_pipeline pip_solid = the_gfx->make_pipeline(
            the_gfx->shader_bindto_pipeline(&shader_solid, &pip_desc_solid, &k_prims3d_vertex_layout_inst));
        sg_pipeline pip_solid_box = the_gfx->make_pipeline(
            the_gfx->shader_bindto_pipeline(&shader_solid_box, &pip_desc_solid, &k_prims3d_vertex_layout_inst));
        sg_pipeline pip_alphablend = the_gfx->make_pipeline(
            the_gfx->shader_bindto_pipeline(&shader_solid, &pip_desc_alphablend, &k_prims3d_vertex_layout_inst));
        sg_pipeline pip_alphablend_box = the_gfx->make_pipeline(
            the_gfx->shader_bindto_pipeline(&shader_solid_box, &pip_desc_alphablend, &k_prims3d_vertex_layout_inst));
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

        g_debug3d.shader = shader_solid.shd;
        g_debug3d.shader_box = shader_solid_box.shd;
        g_debug3d.shader_wire = shader_wire.shd;
        g_debug3d.pip_solid = pip_solid;
        g_debug3d.pip_solid_box = pip_solid_box;
        g_debug3d.pip_alphablend = pip_alphablend;
        g_debug3d.pip_alphablend_box = pip_alphablend_box;
        g_debug3d.pip_wire = pip_wire;
        g_debug3d.pip_wire_ib = pip_wire_ib;
        g_debug3d.pip_wire_strip = pip_wire_strip;

        g_debug3d.dyn_vbuff = the_gfx->make_buffer(&(sg_buffer_desc){
            .type = SG_BUFFERTYPE_VERTEXBUFFER,
            .usage = SG_USAGE_STREAM,
            .size = sizeof(rizz_3d_debug_vertex) * MAX_DYN_VERTICES,
            .label = "debug3d_vbuffer" });
        g_debug3d.dyn_ibuff = the_gfx->make_buffer(&(sg_buffer_desc){
            .type = SG_BUFFERTYPE_INDEXBUFFER,
            .usage = SG_USAGE_STREAM,
            .size = sizeof(rizz_3d_debug_vertex) * MAX_DYN_INDICES,
            .label = "debug3d_ibuffer"});
        g_debug3d.instance_buff = the_gfx->make_buffer(&(sg_buffer_desc) {
            .size = sizeof(debug3d__instance) * MAX_INSTANCES,
            .type = SG_BUFFERTYPE_VERTEXBUFFER,
            .usage = SG_USAGE_STREAM,
            .label = "debug3d_instbuffer"
        });
        if (g_debug3d.dyn_vbuff.id == 0 || g_debug3d.dyn_ibuff.id == 0 || g_debug3d.instance_buff.id == 0) {
            return false;
        }

        // unit shapes 
        {
            rizz_3d_debug_geometry unit_box;
            bool r = debug3d__generate_box_geometry(tmp_alloc, &unit_box, sx_vec3splat(0.5f));
            sx_unused(r);   sx_assert(r);
            g_debug3d.unit_box.vb = the_gfx->make_buffer(&(sg_buffer_desc) {
                .size = unit_box.num_verts * sizeof(rizz_3d_debug_vertex),
                .type = SG_BUFFERTYPE_VERTEXBUFFER,
                .content = unit_box.verts,
                .label = "debug3d_boxshape_vbuff"
            });
            g_debug3d.unit_box.ib = the_gfx->make_buffer(&(sg_buffer_desc) {
                .size = unit_box.num_indices * sizeof(uint16_t),
                .type = SG_BUFFERTYPE_INDEXBUFFER,
                .content = unit_box.indices,
                .label = "debug3d_boxshape_ibuff"
            });
            g_debug3d.unit_box.num_verts = unit_box.num_verts;
            g_debug3d.unit_box.num_indices = unit_box.num_indices;
        }
    
        {
            rizz_3d_debug_geometry unit_sphere;
            bool r = debug3d__generate_sphere_geometry(tmp_alloc, &unit_sphere, 1.0f, 20, 10);
            sx_unused(r);   sx_assert(r);
            g_debug3d.unit_sphere.vb = the_gfx->make_buffer(&(sg_buffer_desc) {
                .size = unit_sphere.num_verts * sizeof(rizz_3d_debug_vertex),
                .type = SG_BUFFERTYPE_VERTEXBUFFER,
                .content = unit_sphere.verts,
                .label = "debug3d_sphereshape_vbuff"
            });
            g_debug3d.unit_sphere.ib = the_gfx->make_buffer(&(sg_buffer_desc) {
                .size = unit_sphere.num_indices * sizeof(uint16_t),
                .type = SG_BUFFERTYPE_INDEXBUFFER,
                .content = unit_sphere.indices,
                .label = "debug3d_sphereshape_ibuff"
            });
            g_debug3d.unit_sphere.num_verts = unit_sphere.num_verts;
            g_debug3d.unit_sphere.num_indices = unit_sphere.num_indices;
        }

        {
            rizz_3d_debug_geometry unit_cone;
            bool r = debug3d__generate_cone_geometry(tmp_alloc, &unit_cone, 20, 0, 1.0f, 1.0f);
            sx_unused(r);
            sx_assert(r);
            g_debug3d.unit_cone.vb = the_gfx->make_buffer(
                &(sg_buffer_desc){ .size = unit_cone.num_verts * sizeof(rizz_3d_debug_vertex),
                                   .type = SG_BUFFERTYPE_VERTEXBUFFER,
                                   .content = unit_cone.verts,
                                   .label = "debug3d_coneshape_vbuff" });
            g_debug3d.unit_cone.ib = the_gfx->make_buffer(
                &(sg_buffer_desc){ .size = unit_cone.num_indices * sizeof(uint16_t),
                                   .type = SG_BUFFERTYPE_INDEXBUFFER,
                                   .content = unit_cone.indices,
                                   .label = "debug3d_coneshape_ibuff" });
            g_debug3d.unit_cone.num_verts = unit_cone.num_verts;
            g_debug3d.unit_cone.num_indices = unit_cone.num_indices;
        }

        // checker texture 
        sx_color checker_colors[] = {
            sx_color4u(200, 200, 200, 255), 
            sx_color4u(255, 255, 255, 255)
        };
        g_debug3d.checker_tex = the_gfx->texture_create_checker(64, 128, checker_colors);

        g_debug3d.max_instances = MAX_INSTANCES;
        g_debug3d.max_verts = MAX_DYN_VERTICES;
        g_debug3d.max_indices = MAX_DYN_INDICES;
    } // scope

    return true;
}

void debug3d__set_draw_api(rizz_api_gfx_draw* draw_api) 
{ 
    g_debug3d.draw_api = draw_api; 
}

static void prims3d__destroy_shape(debug3d__shape* shape) 
{
    if (shape->vb.id) {
        the_gfx->destroy_buffer(shape->vb);
    }
    if (shape->ib.id) {
        the_gfx->destroy_buffer(shape->ib);
    }
}

void debug3d__release(void)
{
    prims3d__destroy_shape(&g_debug3d.unit_box);
    prims3d__destroy_shape(&g_debug3d.unit_sphere);
    prims3d__destroy_shape(&g_debug3d.unit_cone);

    the_gfx->destroy_image(g_debug3d.checker_tex.img);
    the_gfx->destroy_pipeline(g_debug3d.pip_solid);
    the_gfx->destroy_pipeline(g_debug3d.pip_alphablend);
    the_gfx->destroy_pipeline(g_debug3d.pip_alphablend_box);
    the_gfx->destroy_pipeline(g_debug3d.pip_wire);
    the_gfx->destroy_pipeline(g_debug3d.pip_wire_strip);
    the_gfx->destroy_pipeline(g_debug3d.pip_wire_ib);
    the_gfx->destroy_pipeline(g_debug3d.pip_solid_box);
    the_gfx->destroy_buffer(g_debug3d.instance_buff);
    the_gfx->destroy_buffer(g_debug3d.dyn_vbuff);
    the_gfx->destroy_buffer(g_debug3d.dyn_ibuff);
    the_gfx->destroy_shader(g_debug3d.shader);
    the_gfx->destroy_shader(g_debug3d.shader_box);
    the_gfx->destroy_shader(g_debug3d.shader_wire);
}

static inline void debug3d__reset_stats(void)
{
    int64_t frame = the_core->frame_index();
    if (frame != g_debug3d.updated_stats_frame) {
        g_debug3d.updated_stats_frame = frame;
        g_debug3d.num_indices = g_debug3d.num_verts = g_debug3d.num_instances = 0;
    }
}

void debug3d__draw_box(const sx_box* box, const sx_mat4* viewproj_mat,
                       rizz_3d_debug_map_type map_type, sx_color tint)
{
    debug3d__draw_boxes(box, 1, viewproj_mat, map_type, &tint);
}

void debug3d__draw_sphere(sx_vec3 center, float radius, const sx_mat4* viewproj_mat,
                          rizz_3d_debug_map_type map_type, sx_color tint)
{
    debug3d__draw_spheres(&center, &radius, 1, viewproj_mat, map_type, &tint);
}

void debug3d__draw_spheres(const sx_vec3* centers, const float* radiuss, int count, 
                           const sx_mat4* viewproj_mat, rizz_3d_debug_map_type map_type, 
                           const sx_color* tints)
{
    rizz_api_gfx_draw* draw_api = g_debug3d.draw_api;

    debug3d__uniforms uniforms = {
        .viewproj_mat = *viewproj_mat
    };

    // clang-format off
    sg_image map = {0};
    switch (map_type) {
    case RIZZ_3D_DEBUG_MAPTYPE_WHITE:        map = the_gfx->texture_white(); break;
    case RIZZ_3D_DEBUG_MAPTYPE_CHECKER:      map = g_debug3d.checker_tex.img; break;
    }
    // clang-format on

    const sx_alloc* tmp_alloc = the_core->tmp_alloc_push();
    sx_scope(the_core->tmp_alloc_pop()) {
        debug3d__reset_stats();
        sx_assert_always((g_debug3d.num_instances + count) <= g_debug3d.max_instances);
        debug3d__instance* instances = sx_malloc(tmp_alloc, sizeof(debug3d__instance)*count);
        if (!instances) {
            sx_out_of_memory();
            return;
        }

        bool alpha_blend = false;

        for (int i = 0; i < count; i++) {
            sx_vec3 pos = centers[i];
            float radius = radiuss[i];
            debug3d__instance* instance = &instances[i];
            instance->tx1 = sx_vec4f(pos.x, pos.y, pos.z, 1.0f);
            instance->tx2 = sx_vec4f(0, 0, 0,             1.0f);
            instance->tx3 = sx_vec4f(0, 0, 0,             1.0f);
            instance->scale = sx_vec3splat(radius);
            if (tints) {
                instance->color = tints[i];
                alpha_blend = alpha_blend || tints[i].a < 255;
            }
            else {
                instance->color = SX_COLOR_WHITE;
            }
        }

        // in alpha-blend mode (transparent boxes), sort the boxes from back-to-front
        if (alpha_blend) {
            debug3d__instance_depth* sort_items = sx_malloc(tmp_alloc, sizeof(debug3d__instance_depth)*count);
            if (!sort_items) {
                sx_out_of_memory();
                return;
            }
            for (int i = 0; i < count; i++) {
                sort_items[i].index = i;
                sort_items[i].z = sx_mat4_mul_vec3(viewproj_mat, centers[i]).z;
            }

            debug3d__instance_tim_sort(sort_items, count);

            debug3d__instance* sorted = sx_malloc(tmp_alloc, sizeof(debug3d__instance)*count);
            if (!sorted) {
                sx_out_of_memory();
                return;
            }
            for (int i = 0; i < count; i++) {
                sorted[i] = instances[sort_items[i].index];
            }
            instances = sorted;
        }

        int inst_offset = draw_api->append_buffer(g_debug3d.instance_buff, instances,
                                                  sizeof(debug3d__instance) * count);
        g_debug3d.num_instances += count;

        draw_api->apply_pipeline(!alpha_blend ? g_debug3d.pip_solid : g_debug3d.pip_alphablend);
        draw_api->apply_uniforms(SG_SHADERSTAGE_VS, 0, &uniforms, sizeof(uniforms));
        draw_api->apply_bindings(&(sg_bindings) {
            .vertex_buffers[0] = g_debug3d.unit_sphere.vb,
            .vertex_buffers[1] = g_debug3d.instance_buff,
            .vertex_buffer_offsets[1] = inst_offset,
            .index_buffer = g_debug3d.unit_sphere.ib,
            .fs_images[0] = map
        });
        draw_api->draw(0, g_debug3d.unit_sphere.num_indices, count);
    } // scope
}                           

void debug3d__draw_boxes(const sx_box* boxes, int num_boxes, const sx_mat4* viewproj_mat,
                         rizz_3d_debug_map_type map_type, const sx_color* tints)
{
    sx_assert(num_boxes > 0);

    rizz_api_gfx_draw* draw_api = g_debug3d.draw_api;

    debug3d__uniforms uniforms = {
        .viewproj_mat = *viewproj_mat
    };

    sg_image map = {0};
    switch (map_type) {
    case RIZZ_3D_DEBUG_MAPTYPE_WHITE:        map = the_gfx->texture_white(); break;
    case RIZZ_3D_DEBUG_MAPTYPE_CHECKER:      map = g_debug3d.checker_tex.img; break;
    }

    sx_with(const sx_alloc* tmp_alloc = the_core->tmp_alloc_push(), the_core->tmp_alloc_pop()) {
        debug3d__reset_stats();
        sx_assert_always((g_debug3d.num_instances + num_boxes) <= g_debug3d.max_instances);
        debug3d__instance* instances = sx_malloc(tmp_alloc, sizeof(debug3d__instance)*num_boxes);
        if (!instances) {
            sx_out_of_memory();
            return;
        }

        bool alpha_blend = false;

        for (int i = 0; i < num_boxes; i++) {
            const sx_tx3d* tx = &boxes[i].tx;
            debug3d__instance* instance = &instances[i];
            instance->tx1 = sx_vec4f(tx->pos.x, tx->pos.y, tx->pos.z, tx->rot.m11);
            instance->tx2 = sx_vec4f(tx->rot.m21, tx->rot.m31, tx->rot.m12, tx->rot.m22);
            instance->tx3 = sx_vec4f(tx->rot.m32, tx->rot.m13, tx->rot.m23, tx->rot.m33);
            instance->scale = sx_vec3_mulf(boxes[i].e, 2.0f);
            instance->color = tints ? tints[i] : SX_COLOR_WHITE;
            if (tints) {
                instance->color = tints[i];
                alpha_blend = alpha_blend || tints[i].a < 255;
            } else {
                instance->color = SX_COLOR_WHITE;
            }
        }

        // in alpha-blend mode (transparent boxes), sort the boxes from back-to-front
        if (alpha_blend) {
            debug3d__instance_depth* sort_items = sx_malloc(tmp_alloc, sizeof(debug3d__instance_depth)*num_boxes);
            if (!sort_items) {
                sx_out_of_memory();
                return;
            }
            for (int i = 0; i < num_boxes; i++) {
                sort_items[i].index = i;
                sort_items[i].z = sx_mat4_mul_vec3(viewproj_mat, boxes[i].tx.pos).z;
            }

            debug3d__instance_tim_sort(sort_items, num_boxes);

            debug3d__instance* sorted = sx_malloc(tmp_alloc, sizeof(debug3d__instance)*num_boxes);
            if (!sorted) {
                sx_out_of_memory();
                return;
            }
            for (int i = 0; i < num_boxes; i++) {
                sorted[i] = instances[sort_items[i].index];
            }
            instances = sorted;
        }

        int inst_offset = draw_api->append_buffer(g_debug3d.instance_buff, instances,
                                                  sizeof(debug3d__instance) * num_boxes);
        g_debug3d.num_instances += num_boxes;

        draw_api->apply_pipeline(!alpha_blend ? g_debug3d.pip_solid_box : g_debug3d.pip_alphablend_box);
        draw_api->apply_uniforms(SG_SHADERSTAGE_VS, 0, &uniforms, sizeof(uniforms));
        draw_api->apply_bindings(&(sg_bindings) {
            .vertex_buffers[0] = g_debug3d.unit_box.vb,
            .vertex_buffers[1] = g_debug3d.instance_buff,
            .vertex_buffer_offsets[1] = inst_offset,
            .index_buffer = g_debug3d.unit_box.ib,
            .fs_images[0] = map
        });

        draw_api->draw(0, g_debug3d.unit_box.num_indices, num_boxes);
    } // sx_with (temp_alloc)
}

bool debug3d__generate_cone_geometry(const sx_alloc* alloc, rizz_3d_debug_geometry* geo,
                                     int num_segments, float radius1, float radius2, float depth)
{
    sx_assert(depth > 0);

    num_segments = sx_max(num_segments, 4);
    if (num_segments % 2 != 0) {
        ++num_segments; // make it even
    }

    sx_assertf(radius1 > 0 || radius2 > 0, "at least one radius must be more non-zero");
    bool pointy_1 = sx_equal(radius1, 0, 0.00001f);
    bool pointy_2 = sx_equal(radius2, 0, 0.00001f);
    bool pointy = pointy_1 || pointy_2;

    int const num_verts = pointy ? (num_segments + 1) : num_segments*2;
    int const num_indices = pointy ? num_segments*3 : num_segments*6;
    sx_assert(num_verts < UINT16_MAX);

    size_t total_sz = num_verts*sizeof(rizz_3d_debug_vertex) + num_indices*sizeof(uint16_t);
    uint8_t* buff = sx_malloc(alloc, total_sz);
    if (!buff) {
        sx_out_of_memory();
        return false;
    }

    geo->verts = (rizz_3d_debug_vertex*)buff;
    buff += num_verts*sizeof(rizz_3d_debug_vertex);
    geo->indices = (uint16_t*)buff;
    geo->num_verts = num_verts;
    geo->num_indices = num_indices;

    rizz_3d_debug_vertex* verts = geo->verts;
    uint16_t* indices = geo->indices;
    float hdepth = depth * 0.5f;

    if (pointy) {
        float end_z = pointy_1 ? 0 : depth;
        float cap_z = pointy_1 ? depth : 0;
        float radius = pointy_1 ? radius2 : radius1;
        float theta = 0;
        float delta_theta = SX_PI2 / (float)num_segments;

        verts[0].pos = sx_vec3f(0, 0, end_z);
        verts[0].uv = sx_vec2f(0, 1.0f);
        uint16_t vindex = 1;

        for (uint16_t seg = 0; seg < num_segments; seg++) {
            float x = radius * sx_cos(theta);
            float y = radius * sx_sin(theta);
            theta += delta_theta;

            verts[vindex].pos = sx_vec3f(x, y, cap_z);
            verts[vindex].uv = sx_vec2f(sx_cos(theta) * 0.5f + 0.5f, 0);
            ++vindex;
        }
        sx_assert(vindex == (uint16_t)num_verts);

        {   // indices
            uint16_t num_faces = (uint16_t)num_indices/3;
            uint16_t v = 1;
            for (uint16_t i = 0; i < (uint16_t)num_faces; i++) {
                uint16_t iindex = i*3;
                indices[iindex] = 0;
                indices[iindex + 1] = v;
                indices[iindex + 2] = (v + 1) < num_verts ? (v + 1) : ((v + 1)%num_verts + 1);
                v++;
            }

            if (pointy_1) {
                for (uint16_t i = 0; i < (uint16_t)num_faces; i++) {
                    uint16_t iindex = i*3;
                    sx_swap(indices[iindex], indices[iindex+2], uint16_t);
                }
            }
        }
    } else {
        float theta = 0;
        float delta_theta = SX_PI2 / (float)num_segments;
        uint16_t vindex = 0;
        
        uint16_t v1 = 0;
        for (uint16_t seg = 0; seg < (uint16_t)num_segments; seg++) {
            float x = radius1 * sx_cos(theta);
            float y = radius1 * sx_sin(theta);
            theta += delta_theta;

            verts[vindex].pos = sx_vec3f(x, y, hdepth);
            verts[vindex].uv = sx_vec2f(sx_cos(theta) * 0.5f + 0.5f, 0);
            ++vindex;
        }

        uint16_t v2 = vindex;
        uint16_t v2_start = v2;
        for (uint16_t seg = 0; seg < (uint16_t)num_segments; seg++) {
            float x = radius2 * sx_cos(theta);
            float y = radius2 * sx_sin(theta);
            theta += delta_theta;

            verts[vindex].pos = sx_vec3f(x, y, -hdepth);
            verts[vindex].uv = sx_vec2f(sx_cos(theta) * 0.5f + 0.5f, 1.0f);
            ++vindex;
        }

        // indices
        uint16_t num_quads = (uint16_t)num_indices / 6;
        for (uint16_t i = 0; i < num_quads; i++) {
            uint16_t iindex = i * 6;

            indices[iindex] = v1;
            indices[iindex + 1] = v2;
            indices[iindex + 2] = (v2 + 1) < num_verts ? (v2 + 1) : ((v2 + 1)%num_verts);

            indices[iindex + 3] = v1;
            indices[iindex + 4] = (v2 + 1) < num_verts ? (v2 + 1) : ((v2 + 1)%num_verts);
            indices[iindex + 5] = (v1 + 1) < v2 ? (v1 + 1) : ((v1 + 1)%v2_start);

            v1++;
            v2++;
        }
    }

    return true;
}


// TODO: normals are not correct, fix them
bool debug3d__generate_sphere_geometry(const sx_alloc* alloc, rizz_3d_debug_geometry* geo,
                                       float radius, int num_segments, int num_rings)
{
    // coordinate system: right-handed Z-up, winding: CW
    num_segments = sx_max(num_segments, 4);
    num_rings = sx_max(num_rings, 2);
    if (num_segments % 2 != 0) {
        ++num_segments; // make it even
    }
    if (num_rings % 2 == 0) {
        ++num_rings;    // make it odd
    }

    int const num_verts = num_segments*num_rings + 2;
    int const num_indices = 6*num_segments*num_rings;

    sx_assert(num_verts < UINT16_MAX);

    size_t total_sz = num_verts*sizeof(rizz_3d_debug_vertex) + num_indices*sizeof(uint16_t);
    uint8_t* buff = sx_malloc(alloc, total_sz);
    if (!buff) {
        sx_out_of_memory();
        return false;
    }

    geo->verts = (rizz_3d_debug_vertex*)buff;
    buff += num_verts*sizeof(rizz_3d_debug_vertex);
    geo->indices = (uint16_t*)buff;
    geo->num_verts = num_verts;
    geo->num_indices = num_indices;

    rizz_3d_debug_vertex* verts = geo->verts;
    uint16_t* indices = geo->indices;

    verts[0].pos = sx_vec3f(0, 0, -radius);
    verts[num_verts - 1].pos = sx_vec3f(0, 0, radius);

    verts[0].normal = sx_vec3f(0, 0, -1.0f);
    verts[num_verts - 1].normal = sx_vec3f(0, 0, 1.0f);

    uint16_t vindex = 1, iindex = 0;
    float delta_phi = SX_PI / (float)(num_rings + 1);
    float delta_theta = SX_PI2 / (float)num_segments;

    // points
    for (int ring = 0; ring < num_rings; ring++) {
        float phi = delta_phi * (float)(ring + 1);
        float seg_r = radius*sx_sin(phi);
        float z = -radius*sx_cos(phi);
        
        float theta = 0;
        for (int seg = 0; seg < num_segments; seg++) {
            float x = seg_r*sx_cos(theta);
            float y = seg_r*sx_sin(theta);
            theta += delta_theta;            
            
            verts[vindex].pos = sx_vec3f(x, y, z);
            verts[vindex].uv = sx_vec2f(sx_cos(theta)*0.5f+0.5f, sx_cos(phi)*0.5f + 0.5f);
            ++vindex;
        }
    }
    sx_assert(vindex == num_verts - 1);

    // indices
    vindex = 1;
    {   // lower part
        for (uint16_t i = 0, ic = (uint16_t)num_segments; i < ic; i++) {
            indices[iindex++] = 0;
            indices[iindex++] = vindex + i;
            indices[iindex++] = vindex + ((i + 1) % num_segments);
        }

        vindex += (uint16_t)num_segments;
    }

    // quads between lower..upper parts
    for (int ring = 0; ring < num_rings - 1; ring++) {   
        uint16_t num_segs16u = (uint16_t)num_segments;
        uint16_t lower_vindex = vindex - num_segs16u;
        for (uint16_t i = 0; i < num_segs16u; i++) {
            uint16_t next_i = (i + 1)%num_segs16u;
            indices[iindex++] = lower_vindex + i;
            indices[iindex++] = vindex + i;
            indices[iindex++] = vindex + next_i;

            indices[iindex++] = vindex + next_i;
            indices[iindex++] = lower_vindex + next_i;
            indices[iindex++] = lower_vindex + i;
        }
        vindex += (uint16_t)num_segments;
    }

    {   // upper part
        uint16_t num_segs16u = (uint16_t)num_segments;
        vindex -= num_segs16u;
        for (uint16_t i = 0; i < num_segs16u; i++) {
            indices[iindex++] = vindex + i;
            indices[iindex++] = (uint16_t)num_verts - 1;
            indices[iindex++] = vindex + ((i + 1) % num_segs16u);
        }
    }

    // colors
    for (int i = 0; i < num_verts; i++) {
        verts[i].color = SX_COLOR_WHITE;
    }

    // normals
    const sx_alloc* tmp_alloc = the_core->tmp_alloc_push();
    sx_scope(the_core->tmp_alloc_pop()) {
        sx_vec3* face_normals = sx_malloc(tmp_alloc, sizeof(sx_vec3)*(num_indices/3));
        sx_assert_always(face_normals);

        typedef struct adjucent_vertex {
            int count;
            int faces[6];
        } adjucent_vertex;   

        adjucent_vertex* adjucency = sx_malloc(tmp_alloc, sizeof(adjucent_vertex)*num_verts);
        sx_assert_always(adjucency);
        sx_memset(adjucency, 0x0, sizeof(adjucent_vertex)*num_verts);

        for (int i = 0; i < num_indices; i+=3) {
            sx_vec3 face_normal = sx_plane_normal(verts[indices[i]].pos, verts[indices[i + 1]].pos,
                                                  verts[indices[i + 2]].pos);

            sx_vec3 v = verts[indices[i]].pos;
            sx_assert(sx_vec3_dot(face_normal, v) >= 0);

            int face_id = i/3;
            face_normals[face_id] = face_normal;

            if (indices[i] != 0 && indices[i] != num_verts - 1) {
                adjucent_vertex* a1 = &adjucency[indices[i]];
                sx_assert(a1->count <= 6);
                a1->faces[a1->count++] = face_id;
            }

            if (indices[i+1] != 0 && indices[i+1] != num_verts - 1) {
                adjucent_vertex* a2 = &adjucency[indices[i + 1]];
                sx_assert(a2->count <= 6);
                a2->faces[a2->count++] = face_id;
            }

            if (indices[i+2] != 0 && indices[i+2] != num_verts - 1) {
                adjucent_vertex* a3 = &adjucency[indices[i + 2]];
                sx_assert(a3->count <= 6);
                a3->faces[a3->count++] = face_id;
            }
        }

        for (int i = 1; i < num_verts - 1; i++) {
            adjucent_vertex* a = &adjucency[i];
            sx_vec3 normal = SX_VEC3_ZERO;
            for (int k = 0; k < a->count; k++) {
                normal = sx_vec3_add(normal, face_normals[a->faces[k]]);
            }
            normal = sx_vec3_norm(sx_vec3_mulf(normal, 1.0f/(float)a->count));
            verts[i].normal = normal;
        }
    } // scope

    sx_assert(iindex == num_indices);
    return true;
}

 bool debug3d__generate_box_geometry(const sx_alloc* alloc, rizz_3d_debug_geometry* geo, sx_vec3 extents)
{
    // coordinate system: right-handed Z-up
    // winding: CW
    const int num_verts = 24;
    const int num_indices = 36;
    size_t total_sz = num_verts*sizeof(rizz_3d_debug_vertex) * num_indices*sizeof(uint16_t);
    uint8_t* buff = sx_malloc(alloc, total_sz);
    if (!buff) {
        sx_out_of_memory();
        return false;
    }

    geo->verts = (rizz_3d_debug_vertex*)buff;
    buff += num_verts*sizeof(rizz_3d_debug_vertex);
    geo->indices = (uint16_t*)buff;
    geo->num_verts = num_verts;
    geo->num_indices = num_indices;

    rizz_3d_debug_vertex* verts = geo->verts;
    uint16_t* indices = geo->indices;

    sx_aabb a = sx_aabbf(-extents.x, -extents.y, -extents.z, extents.x,  extents.y,  extents.z);
    sx_vec3 corners[8];
    sx_aabb_corners(corners, &a);
    
    // X+
    verts[0] = (rizz_3d_debug_vertex) { .pos = corners[1], .normal = sx_vec3f(1.0f, 0, 0), .uv = sx_vec2f(0, 1.0f), .color = sx_colorn(0xffffffff) };
    verts[1] = (rizz_3d_debug_vertex) { .pos = corners[3], .normal = sx_vec3f(1.0f, 0, 0), .uv = sx_vec2f(0, 0), .color = sx_colorn(0xffffffff) };
    verts[2] = (rizz_3d_debug_vertex) { .pos = corners[7], .normal = sx_vec3f(1.0f, 0, 0), .uv = sx_vec2f(1.0f, 0), .color = sx_colorn(0xffffffff) };
    verts[3] = (rizz_3d_debug_vertex) { .pos = corners[5], .normal = sx_vec3f(1.0f, 0, 0), .uv = sx_vec2f(1.0f, 1.0f), .color = sx_colorn(0xffffffff) };
    indices[0] = 0;         indices[1] = 1;         indices[2] = 3;
    indices[3] = 1;         indices[4] = 2;         indices[5] = 3;

    // X-
    verts[4] = (rizz_3d_debug_vertex) { .pos = corners[4], .normal = sx_vec3f(-1.0f, 0, 0), .uv = sx_vec2f(0, 1.0f), .color = sx_colorn(0xffffffff) };
    verts[5] = (rizz_3d_debug_vertex) { .pos = corners[6], .normal = sx_vec3f(-1.0f, 0, 0), .uv = sx_vec2f(0, 0), .color = sx_colorn(0xffffffff) };
    verts[6] = (rizz_3d_debug_vertex) { .pos = corners[2], .normal = sx_vec3f(-1.0f, 0, 0), .uv = sx_vec2f(1.0f, 0), .color = sx_colorn(0xffffffff) };
    verts[7] = (rizz_3d_debug_vertex) { .pos = corners[0], .normal = sx_vec3f(-1.0f, 0, 0), .uv = sx_vec2f(1.0f, 1.0f), .color = sx_colorn(0xffffffff) };
    indices[6] = 4;         indices[7] = 5;         indices[8] = 7;
    indices[9] = 5;         indices[10] = 6;        indices[11] = 7;

    // Y-
    verts[8] = (rizz_3d_debug_vertex) { .pos = corners[0], .normal = sx_vec3f(0, -1.0f, 0), .uv = sx_vec2f(0, 1.0f), .color = sx_colorn(0xffffffff) };
    verts[9] = (rizz_3d_debug_vertex) { .pos = corners[2], .normal = sx_vec3f(0, -1.0f, 0), .uv = sx_vec2f(0, 0), .color = sx_colorn(0xffffffff) };
    verts[10] = (rizz_3d_debug_vertex) { .pos = corners[3], .normal = sx_vec3f(0, -1.0f, 0), .uv = sx_vec2f(1.0f, 0), .color = sx_colorn(0xffffffff) };
    verts[11] = (rizz_3d_debug_vertex) { .pos = corners[1], .normal = sx_vec3f(0, -1.0f, 0), .uv = sx_vec2f(1.0f, 1.0f), .color = sx_colorn(0xffffffff) };
    indices[12] = 8;       indices[13] = 9;        indices[14] = 11;
    indices[15] = 9;       indices[16] = 10;       indices[17] = 11;

    // Y+
    verts[12] = (rizz_3d_debug_vertex) { .pos = corners[5], .normal = sx_vec3f(0, 1.0f, 0), .uv = sx_vec2f(0, 1.0f), .color = sx_colorn(0xffffffff) };
    verts[13] = (rizz_3d_debug_vertex) { .pos = corners[7], .normal = sx_vec3f(0, 1.0f, 0), .uv = sx_vec2f(0, 0), .color = sx_colorn(0xffffffff) };
    verts[14] = (rizz_3d_debug_vertex) { .pos = corners[6], .normal = sx_vec3f(0, 1.0f, 0), .uv = sx_vec2f(1.0f, 0), .color = sx_colorn(0xffffffff) };
    verts[15] = (rizz_3d_debug_vertex) { .pos = corners[4], .normal = sx_vec3f(0, 1.0f, 0), .uv = sx_vec2f(1.0f, 1.0f), .color = sx_colorn(0xffffffff) };
    indices[18] = 12;      indices[19] = 13;      indices[20] = 15;
    indices[21] = 13;      indices[22] = 14;      indices[23] = 15;

    // Z-
    verts[16] = (rizz_3d_debug_vertex) { .pos = corners[1], .normal = sx_vec3f(0, 0, -1.0f), .uv = sx_vec2f(0, 1.0f), .color = sx_colorn(0xffffffff) };
    verts[17] = (rizz_3d_debug_vertex) { .pos = corners[5], .normal = sx_vec3f(0, 0, -1.0f), .uv = sx_vec2f(0, 0), .color = sx_colorn(0xffffffff) };
    verts[18] = (rizz_3d_debug_vertex) { .pos = corners[4], .normal = sx_vec3f(0, 0, -1.0f), .uv = sx_vec2f(1.0f, 0), .color = sx_colorn(0xffffffff) };
    verts[19] = (rizz_3d_debug_vertex) { .pos = corners[0], .normal = sx_vec3f(0, 0, -1.0f), .uv = sx_vec2f(1.0f, 1.0f), .color = sx_colorn(0xffffffff) };
    indices[24] = 16;      indices[25] = 17;      indices[26] = 19;
    indices[27] = 17;      indices[28] = 18;      indices[29] = 19;

    // Z+
    verts[20] = (rizz_3d_debug_vertex) { .pos = corners[2], .normal = sx_vec3f(0, 0, 1.0f), .uv = sx_vec2f(0, 1.0f), .color = sx_colorn(0xffffffff) };
    verts[21] = (rizz_3d_debug_vertex) { .pos = corners[6], .normal = sx_vec3f(0, 0, 1.0f), .uv = sx_vec2f(0, 0), .color = sx_colorn(0xffffffff) };
    verts[22] = (rizz_3d_debug_vertex) { .pos = corners[7], .normal = sx_vec3f(0, 0, 1.0f), .uv = sx_vec2f(1.0f, 0), .color = sx_colorn(0xffffffff) };
    verts[23] = (rizz_3d_debug_vertex) { .pos = corners[3], .normal = sx_vec3f(0, 0, 1.0f), .uv = sx_vec2f(1.0f, 1.0f), .color = sx_colorn(0xffffffff) };
    indices[30] = 20;      indices[31] = 21;      indices[32] = 23;
    indices[33] = 21;      indices[34] = 22;      indices[35] = 23;    

    return true;
}

void debug3d__free_geometry(rizz_3d_debug_geometry* geo, const sx_alloc* alloc)
{
    sx_assert(geo);
    sx_free(alloc, geo->verts);
}

void debug3d__draw_aabb(const sx_aabb* aabb, const sx_mat4* viewproj_mat, sx_color tint)
{
    debug3d__draw_aabbs(aabb, 1, viewproj_mat, &tint);
}

void debug3d__draw_aabbs(const sx_aabb* aabbs, int num_aabbs, const sx_mat4* viewproj_mat, const sx_color* tints)
{
    sx_assert(aabbs);
    sx_assert(num_aabbs > 0);
    
    const int num_verts = 8 * num_aabbs;
    const int num_indices = 24 * num_aabbs;
    rizz_api_gfx_draw* draw_api = g_debug3d.draw_api;
    sx_vec3 corners[8];

    debug3d__reset_stats();
    sx_assert_always((g_debug3d.num_verts + num_verts) <= g_debug3d.max_verts);
    sx_assert_always((g_debug3d.num_verts + num_indices) <= g_debug3d.max_indices);

    const sx_alloc* tmp_alloc = the_core->tmp_alloc_push();
    sx_scope(the_core->tmp_alloc_pop()) {
        size_t total_sz = num_verts*sizeof(rizz_3d_debug_vertex) + num_indices*sizeof(uint16_t);
        uint8_t* buff = sx_malloc(tmp_alloc, total_sz);
        if (!buff) {
            sx_out_of_memory();
            return;
        }

        rizz_3d_debug_vertex* vertices = (rizz_3d_debug_vertex*)buff;
        buff += sizeof(rizz_3d_debug_vertex)*num_verts;
        uint16_t* indices = (uint16_t*)buff;

        rizz_3d_debug_vertex* _verts = vertices;
        uint16_t* _indices = indices;
        for (int i = 0; i < num_aabbs; i++) {
            sx_aabb_corners(corners, &aabbs[i]);
            for (int vi = 0; vi < 8; vi++) {
                _verts[vi].pos = corners[vi];
                _verts[vi].color = tints ? tints[i] : SX_COLOR_WHITE;
            }
            _verts += 8;
        
            int vindex = i*8;
            sx_assert(vindex < UINT16_MAX);
            uint16_t vindexu16 = (uint16_t)vindex;
            _indices[0] = vindexu16;             _indices[1] = vindexu16 + 1;           
            _indices[2] = vindexu16 + 1;         _indices[3] = vindexu16 + 3;
            _indices[4] = vindexu16 + 3;         _indices[5] = vindexu16 + 2;
            _indices[6] = vindexu16 + 2;         _indices[7] = vindexu16 + 0;
            _indices[8] = vindexu16 + 5;         _indices[9] = vindexu16 + 4;
            _indices[10] = vindexu16 + 4;        _indices[11] = vindexu16 + 6;
            _indices[12] = vindexu16 + 6;        _indices[13] = vindexu16 + 7;
            _indices[14] = vindexu16 + 7;        _indices[15] = vindexu16 + 5;
            _indices[16] = vindexu16 + 5;        _indices[17] = vindexu16 + 1;
            _indices[18] = vindexu16 + 7;        _indices[19] = vindexu16 + 3;
            _indices[20] = vindexu16 + 0;        _indices[21] = vindexu16 + 4;
            _indices[22] = vindexu16 + 2;        _indices[23] = vindexu16 + 6;

            _indices += 24;
        }

        int vb_offset = draw_api->append_buffer(g_debug3d.dyn_vbuff, vertices, num_verts * sizeof(rizz_3d_debug_vertex));
        int ib_offset = draw_api->append_buffer(g_debug3d.dyn_ibuff, indices, num_indices * sizeof(uint16_t));
        g_debug3d.num_verts += num_verts;
        g_debug3d.num_indices += num_indices;

        sg_bindings bind = { 
            .vertex_buffers[0] = g_debug3d.dyn_vbuff, 
            .index_buffer = g_debug3d.dyn_ibuff,
            .vertex_buffer_offsets[0] = vb_offset,
            .index_buffer_offset = ib_offset };

        draw_api->apply_pipeline(g_debug3d.pip_wire_ib);
        draw_api->apply_uniforms(SG_SHADERSTAGE_VS, 0, viewproj_mat, sizeof(*viewproj_mat));
        draw_api->apply_bindings(&bind);
        draw_api->draw(0, num_indices, 1);
    } // scope
}

void debug3d__grid_xzplane(float spacing, float spacing_bold, const sx_mat4* vp, const sx_vec3 frustum[8])
{
    static const sx_color color = { { 170, 170, 170, 255 } };
    static const sx_color bold_color = { { 255, 255, 255, 255 } };

    rizz_api_gfx_draw* draw_api = g_debug3d.draw_api;

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

    debug3d__reset_stats();
    sx_assert_always((g_debug3d.num_verts + num_verts) <= g_debug3d.max_verts);

    // draw
    int data_size = num_verts * sizeof(rizz_3d_debug_vertex);
    
    const sx_alloc* tmp_alloc = the_core->tmp_alloc_push();
    sx_scope(the_core->tmp_alloc_pop()) {
        rizz_3d_debug_vertex* verts = sx_malloc(tmp_alloc, data_size);
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

        int offset = draw_api->append_buffer(g_debug3d.dyn_vbuff, verts, data_size);
        g_debug3d.num_verts += num_verts;

        sg_bindings bind = { 
            .vertex_buffers[0] = g_debug3d.dyn_vbuff, 
            .vertex_buffer_offsets[0] = offset };

        draw_api->apply_pipeline(g_debug3d.pip_wire);
        draw_api->apply_uniforms(SG_SHADERSTAGE_VS, 0, vp, sizeof(*vp));
        draw_api->apply_bindings(&bind);
        draw_api->draw(0, num_verts, 1);
    } // sx_scope
}

void debug3d__grid_xyplane(float spacing, float spacing_bold, const sx_mat4* vp, const sx_vec3 frustum[8])
{
    static const sx_color color = { { 170, 170, 170, 255 } };
    static const sx_color bold_color = { { 255, 255, 255, 255 } };

    rizz_api_gfx_draw* draw_api = g_debug3d.draw_api;

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

    debug3d__reset_stats();
    sx_assert_always((g_debug3d.num_verts + num_verts) <= g_debug3d.max_verts);

    // draw
    int data_size = num_verts * sizeof(rizz_3d_debug_vertex);
    
    const sx_alloc* tmp_alloc = the_core->tmp_alloc_push();
    sx_scope(the_core->tmp_alloc_pop()) {
        rizz_3d_debug_vertex* verts = sx_malloc(tmp_alloc, data_size);
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

        int offset = draw_api->append_buffer(g_debug3d.dyn_vbuff, verts, data_size);
        g_debug3d.num_verts += num_verts;

        sg_bindings bind = { 
            .vertex_buffers[0] = g_debug3d.dyn_vbuff, 
            .vertex_buffer_offsets[0] = offset };

        draw_api->apply_pipeline(g_debug3d.pip_wire);
        draw_api->apply_uniforms(SG_SHADERSTAGE_VS, 0, vp, sizeof(*vp));
        draw_api->apply_bindings(&bind);
        draw_api->draw(0, num_verts, 1);
    } // scope
}

void debug3d__grid_xyplane_cam(float spacing, float spacing_bold, float dist, const rizz_camera* cam, 
                               const sx_mat4* viewproj_mat)
{
    sx_vec3 frustum[8];
    the_camera->calc_frustum_points_range(cam, frustum, -dist, dist);
    debug3d__grid_xyplane(spacing, spacing_bold, viewproj_mat, frustum);
}

void debug3d__set_max_instances(int max_instances)
{
    sx_assert(max_instances > 0);

    if (g_debug3d.instance_buff.id) {
        the_gfx->destroy_buffer(g_debug3d.instance_buff);
    }

    g_debug3d.instance_buff = the_gfx->make_buffer(&(sg_buffer_desc) {
        .size = sizeof(debug3d__instance) * max_instances,
        .type = SG_BUFFERTYPE_VERTEXBUFFER,
        .usage = SG_USAGE_STREAM,
        .label = "debug3d_instbuff"
    });

    sx_assert_always(g_debug3d.instance_buff.id);
    g_debug3d.max_instances = max_instances;
}

void debug3d__set_max_vertices(int max_verts)
{
    sx_assert(max_verts > 0);

    if (g_debug3d.dyn_vbuff.id) {
        the_gfx->destroy_buffer(g_debug3d.dyn_vbuff);
    }

    g_debug3d.dyn_vbuff = the_gfx->make_buffer(&(sg_buffer_desc){
        .type = SG_BUFFERTYPE_VERTEXBUFFER,
        .usage = SG_USAGE_STREAM,
        .size = sizeof(rizz_3d_debug_vertex) * max_verts,
        .label = "debug3d_vbuff" });

    sx_assert_always(g_debug3d.instance_buff.id);
    g_debug3d.max_verts = max_verts;
}

void debug3d__set_max_indices(int max_indices)
{
    sx_assert(max_indices > 0);

    if (g_debug3d.dyn_ibuff.id) {
        the_gfx->destroy_buffer(g_debug3d.dyn_ibuff);
    }

    g_debug3d.dyn_ibuff = the_gfx->make_buffer(&(sg_buffer_desc){
        .type = SG_BUFFERTYPE_INDEXBUFFER,
        .usage = SG_USAGE_STREAM,
        .size = sizeof(rizz_3d_debug_vertex) * max_indices,
        .label = "debug3d_ibuff" });

    sx_assert_always(g_debug3d.dyn_ibuff.id);
    g_debug3d.max_indices = max_indices;
}

void debug3d__draw_path(const sx_vec3* points, int num_points, const sx_mat4* viewproj_mat, const sx_color color)
{
    int const num_verts = num_points;
    int const data_size = num_verts * sizeof(rizz_3d_debug_vertex);
    rizz_api_gfx_draw* draw_api = g_debug3d.draw_api;

    debug3d__reset_stats();
    sx_assert_always((g_debug3d.num_verts + num_verts) <= g_debug3d.max_verts);

    const sx_alloc* tmp_alloc = the_core->tmp_alloc_push();
    sx_scope(the_core->tmp_alloc_pop()) {
        rizz_3d_debug_vertex* verts = sx_malloc(tmp_alloc, data_size);

        for (int i = 0; i < num_points; i++) {
            verts[i].pos = points[i];
            verts[i].color = color;
        }

        int offset = draw_api->append_buffer(g_debug3d.dyn_vbuff, verts, data_size);
        g_debug3d.num_verts += num_verts;
        sg_bindings bind = { 
            .vertex_buffers[0] = g_debug3d.dyn_vbuff, 
            .vertex_buffer_offsets[0] = offset };

        draw_api->apply_pipeline(g_debug3d.pip_wire_strip);
        draw_api->apply_uniforms(SG_SHADERSTAGE_VS, 0, viewproj_mat, sizeof(*viewproj_mat));
        draw_api->apply_bindings(&bind);
        draw_api->draw(0, num_verts, 1);
    } // sx_scope
}

void debug3d__draw_lines(int num_lines, const rizz_3d_debug_line* lines, const sx_mat4* viewproj_mat, 
                         const sx_color* colors)
{
    const int num_verts = 2*num_lines;
    const int data_size = num_verts * sizeof(rizz_3d_debug_vertex);
    rizz_api_gfx_draw* draw_api = g_debug3d.draw_api;

    debug3d__reset_stats();
    sx_assert_always((g_debug3d.num_verts + num_verts) <= g_debug3d.max_verts);

    const sx_alloc* tmp_alloc = the_core->tmp_alloc_push();
    sx_scope(the_core->tmp_alloc_pop()) {
        rizz_3d_debug_vertex* verts = sx_malloc(tmp_alloc, data_size);

        for (int i = 0; i < num_lines; i++) {
            sx_color c = colors ? colors[i] : sx_colorn(0xffffffff);
            int index = i*2;

            verts[index].pos = lines[i].p0;
            verts[index].color = c;

            verts[index+1].pos = lines[i].p1;
            verts[index+1].color = c;
        }

        int offset = draw_api->append_buffer(g_debug3d.dyn_vbuff, verts, data_size);
        g_debug3d.num_verts += num_verts;
        sg_bindings bind = { 
            .vertex_buffers[0] = g_debug3d.dyn_vbuff, 
            .vertex_buffer_offsets[0] = offset };

        draw_api->apply_pipeline(g_debug3d.pip_wire);
        draw_api->apply_uniforms(SG_SHADERSTAGE_VS, 0, viewproj_mat, sizeof(*viewproj_mat));
        draw_api->apply_bindings(&bind);
        draw_api->draw(0, num_verts, 1);
    } // sx_scope
}

void debug3d__draw_line(const sx_vec3 p0, const sx_vec3 p1, const sx_mat4* viewproj_mat, const sx_color color)
{
    debug3d__draw_lines(1, &(rizz_3d_debug_line){ .p0 = p0, .p1 = p1}, viewproj_mat, &color);
}

void debug3d__draw_cones(const float* radiuss, const float* depths, const sx_tx3d* txs, int count, 
                         const sx_mat4* viewproj_mat, const sx_color* tints)
{
    rizz_api_gfx_draw* draw_api = g_debug3d.draw_api;

    debug3d__uniforms uniforms = {
        .viewproj_mat = *viewproj_mat
    };

    const sx_alloc* tmp_alloc = the_core->tmp_alloc_push();
    sx_scope(the_core->tmp_alloc_pop()) {

        debug3d__reset_stats();
        sx_assert_always((g_debug3d.num_instances + count) <= g_debug3d.max_instances);
        debug3d__instance* instances = sx_malloc(tmp_alloc, sizeof(debug3d__instance)*count);
        sx_assert_always(instances);

        bool alpha_blend = false;

        for (int i = 0; i < count; i++) {
            const sx_tx3d* tx = &txs[i];
            float radius = radiuss[i];
            float depth = depths[i];

            debug3d__instance* instance = &instances[i];
            instance->tx1 = sx_vec4f(tx->pos.x, tx->pos.y, tx->pos.z, tx->rot.m11);
            instance->tx2 = sx_vec4f(tx->rot.m21, tx->rot.m31, tx->rot.m12, tx->rot.m22);
            instance->tx3 = sx_vec4f(tx->rot.m23, tx->rot.m13, tx->rot.m23, tx->rot.m33);
            instance->scale = sx_vec3f(radius, radius, depth);
            if (tints) {
                instance->color = tints[i];
                alpha_blend = alpha_blend || tints[i].a < 255;
            }
            else {
                instance->color = SX_COLOR_WHITE;
            }
        }

        // in alpha-blend mode (transparent boxes), sort the boxes from back-to-front
        if (alpha_blend) {
            debug3d__instance_depth* sort_items = sx_malloc(tmp_alloc, sizeof(debug3d__instance_depth)*count);
            if (!sort_items) {
                sx_out_of_memory();
                return;
            }
            for (int i = 0; i < count; i++) {
                sort_items[i].index = i;
                sort_items[i].z = sx_mat4_mul_vec3(viewproj_mat, txs[i].pos).z;
            }

            debug3d__instance_tim_sort(sort_items, count);

            debug3d__instance* sorted = sx_malloc(tmp_alloc, sizeof(debug3d__instance)*count);
            if (!sorted) {
                sx_out_of_memory();
                return;
            }
            for (int i = 0; i < count; i++) {
                sorted[i] = instances[sort_items[i].index];
            }
            instances = sorted;
        }

        int inst_offset = draw_api->append_buffer(g_debug3d.instance_buff, instances,
                                                  sizeof(debug3d__instance) * count);
        g_debug3d.num_instances += count;

        draw_api->apply_pipeline(!alpha_blend ? g_debug3d.pip_solid : g_debug3d.pip_alphablend);
        draw_api->apply_uniforms(SG_SHADERSTAGE_VS, 0, &uniforms, sizeof(uniforms));
        draw_api->apply_bindings(&(sg_bindings) {
            .vertex_buffers[0] = g_debug3d.unit_cone.vb,
            .vertex_buffers[1] = g_debug3d.instance_buff,
            .vertex_buffer_offsets[1] = inst_offset,
            .index_buffer = g_debug3d.unit_cone.ib,
            .fs_images[0] = the_gfx->texture_white()
        });
        draw_api->draw(0, g_debug3d.unit_cone.num_indices, count);
    } // sx_scope
}

void debug3d__draw_cone(float radius, float depth, const sx_tx3d* tx, const sx_mat4* viewproj_mat, sx_color tint)
{
    debug3d__draw_cones(&radius, &depth, tx, 1, viewproj_mat, &tint);
}

void debug3d__draw_axis(const sx_mat4* mat, const sx_mat4* viewproj_mat, float scale)
{
    if (scale <= 0) {
        scale = 1.0f;
    }

    sx_vec3 xaxis = sx_vec3fv(mat->fc1);
    sx_vec3 yaxis = sx_vec3fv(mat->fc2);
    sx_vec3 zaxis = sx_vec3fv(mat->fc3);
    sx_vec3 p = sx_vec3fv(mat->fc4);
    rizz_3d_debug_line lines[3] = {
        {
            .p0 = p,
            .p1 = sx_vec3_add(p, sx_vec3_mulf(xaxis, scale))
        },
        {
            .p0 = p,
            .p1 = sx_vec3_add(p, sx_vec3_mulf(yaxis, scale))
        },
        {
            .p0 = p,
            .p1 = sx_vec3_add(p, sx_vec3_mulf(zaxis, scale))
        }
    };
    sx_color colors[3] = {
        sx_color4u(255, 0, 0, 255),
        sx_color4u(0, 255, 0, 255),
        sx_color4u(0, 0, 255, 255)
    };

    debug3d__draw_lines(3, lines, viewproj_mat, colors);
}

void debug3d__draw_camera(const rizz_camera* cam, const sx_mat4* viewproj_mat)
{
    sx_assert(cam);

    sx_mat4 cam_mat = sx_mat4v(sx_vec4v3(cam->right, 0), 
                               sx_vec4v3(cam->up, 0), 
                               sx_vec4v3(cam->forward, 0), 
                               sx_vec4v3(cam->pos, 1.0f));
    debug3d__draw_axis(&cam_mat, viewproj_mat, 0.5f);

    sx_vec3 frustum_pts[8];
    the_camera->calc_frustum_points(cam, frustum_pts);

    sx_vec3 near_plane[5] = {
        frustum_pts[0],
        frustum_pts[1],
        frustum_pts[2],
        frustum_pts[3],
        frustum_pts[0],
    };

    sx_vec3 far_plane[5] = {
        frustum_pts[4],
        frustum_pts[5],
        frustum_pts[6],
        frustum_pts[7],
        frustum_pts[4],
    };

    debug3d__draw_path(near_plane, 5, viewproj_mat, SX_COLOR_WHITE);
    debug3d__draw_path(far_plane, 5, viewproj_mat, SX_COLOR_WHITE);

    sx_plane _p = sx_plane3p(frustum_pts[0], frustum_pts[1], frustum_pts[2]);
    sx_unused(_p);
    
    rizz_3d_debug_line lines1[4] = { { .p0 = frustum_pts[0], .p1 = frustum_pts[4] },
                                     { .p0 = frustum_pts[1], .p1 = frustum_pts[7] },
                                     { .p0 = frustum_pts[2], .p1 = frustum_pts[6] },
                                     { .p0 = frustum_pts[3], .p1 = frustum_pts[5] } };
    debug3d__draw_lines(4, lines1, viewproj_mat, NULL);
    
    rizz_3d_debug_line lines2[4] = { { .p0 = cam->pos, .p1 = frustum_pts[0] },
                                     { .p0 = cam->pos, .p1 = frustum_pts[1] },
                                     { .p0 = cam->pos, .p1 = frustum_pts[2] },
                                     { .p0 = cam->pos, .p1 = frustum_pts[3] } };
    const sx_color colors2[] = {
        sx_color4u(100, 100, 100, 255),
        sx_color4u(100, 100, 100, 255),
        sx_color4u(100, 100, 100, 255),
        sx_color4u(100, 100, 100, 255)
    };
    debug3d__draw_lines(4, lines2, viewproj_mat, colors2);

}


