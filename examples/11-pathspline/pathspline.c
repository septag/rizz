#include "rizz/config.h"
#include "rizz/utility.h"
#include "sx/allocator.h"
#include "sx/io.h"
#include "sx/math-vec.h"
#include "sx/os.h"
#include "sx/rng.h"
#include "sx/string.h"

#include "rizz/3dtools.h"
#include "rizz/imgui-extra.h"
#include "rizz/imgui.h"
#include "rizz/rizz.h"

#include <alloca.h>

#include "../common.h"

RIZZ_STATE static rizz_api_core* the_core;
RIZZ_STATE static rizz_api_gfx* the_gfx;
RIZZ_STATE static rizz_api_app* the_app;
RIZZ_STATE static rizz_api_imgui* the_imgui;
RIZZ_STATE static rizz_api_asset* the_asset;
RIZZ_STATE static rizz_api_camera* the_camera;
RIZZ_STATE static rizz_api_vfs* the_vfs;
RIZZ_STATE static rizz_api_3d* the_3d;
RIZZ_STATE static rizz_api_imgui_extra* the_imguix;
RIZZ_STATE static rizz_api_utility* the_utility;

#ifndef EXAMPLES_ROOT
#    define EXAMPLES_ROOT ""
#endif

typedef struct rizz_model rizz_model;

typedef struct {
    sx_mat4 viewproj_mat;
    sx_mat4 world_mat;
} draw3d_vertex_shader_uniforms;

typedef struct {
    sx_vec3 light_dir;
    float _reserved;
    sx_vec3 color;
    float _reserved2;
} draw3d_fragment_shader_uniforms;

typedef struct {
    rizz_spline3d_node base;
    sx_quat rot;
    float fov;
} cam_spline_node;

typedef struct {
    sx_rng rng;
    rizz_gfx_stage stage;
    rizz_camera cam;
    rizz_asset shader;
    rizz_asset cube_model;
    sg_pipeline pip;
    sx_vec3 light_dir;
    sx_vec3 cube_pos;
    cam_spline_node cam_evalresult;
    sx_quat cam_rot;
    rizz_spline3d_node cube_path[4];
    cam_spline_node cam_path[4];
    sx_mat4 world_mat;
    rizz_texture checker_tex;
} draw3d_state;

RIZZ_STATE static draw3d_state g_draw3d;

typedef struct vertex_stream1 {
    sx_vec3 pos;
} vertex_stream1;

typedef struct vertex_stream2 {
    sx_vec3 normal;
    sx_vec3 tangent;
    sx_vec3 bitangent;
} vertex_stream2;

typedef struct vertex_stream3 {
    sx_vec2 uv;
    sx_color color;
} vertex_stream3;

static const rizz_model_geometry_layout k_stream_layout = {
    .attrs[0] = { .semantic = "POSITION",
                  .offset = offsetof(vertex_stream1, pos),
                  .buffer_index = 0,
                  .format = SG_VERTEXFORMAT_FLOAT3 },
    .attrs[1] = { .semantic = "NORMAL",
                  .offset = offsetof(vertex_stream2, normal),
                  .buffer_index = 1,
                  .format = SG_VERTEXFORMAT_FLOAT3 },
    .attrs[2] = { .semantic = "TANGENT",
                  .offset = offsetof(vertex_stream2, tangent),
                  .buffer_index = 1,
                  .format = SG_VERTEXFORMAT_FLOAT3 },
    .attrs[3] = { .semantic = "BINORMAL",
                  .offset = offsetof(vertex_stream2, bitangent),
                  .buffer_index = 1,
                  .format = SG_VERTEXFORMAT_FLOAT3 },
    .attrs[4] = { .semantic = "TEXCOORD",
                  .offset = offsetof(vertex_stream3, uv),
                  .buffer_index = 2,
                  .format = SG_VERTEXFORMAT_FLOAT2 },
    .attrs[5] = { .semantic = "COLOR",
                  .offset = offsetof(vertex_stream3, color),
                  .buffer_index = 2,
                  .format = SG_VERTEXFORMAT_UBYTE4N },
    .buffer_strides[0] = sizeof(vertex_stream1),
    .buffer_strides[1] = sizeof(vertex_stream2),
    .buffer_strides[2] = sizeof(vertex_stream3)
};

static sx_quat quat_lookat(sx_vec3 pos, sx_vec3 target)
{
    sx_vec3 y = sx_vec3_norm(sx_vec3_sub(target, pos));
    sx_vec3 x = sx_vec3_norm(sx_vec3_cross(y, SX_VEC3_UNITZ));
    sx_vec3 z = sx_vec3_cross(x, y);

    sx_mat4 m = sx_mat4v(sx_vec4v3(x, 0), sx_vec4v3(y, 0), sx_vec4v3(z, 0), SX_VEC4_ZERO);
    return sx_mat4_quat(&m);
}

static inline float ease_inout(float x)
{
    if (x < 0.5f)
        return 2 * x * x;

    x -= 1;
    return 1 - 2 * x * x;
}

static void cam_spline_eval(const rizz_spline3d_node* n1, const rizz_spline3d_node* n2, float t,
                            sx_vec3* r)
{
    t = ease_inout(t);
    const cam_spline_node* node1 = (cam_spline_node*)n1;
    const cam_spline_node* node2 = (cam_spline_node*)n2;
    cam_spline_node* result = (cam_spline_node*)r;
    result->fov = sx_lerp(node1->fov, node2->fov, t);
    result->rot = sx_quat_lerp(node1->rot, node2->rot, t);
}

static bool init()
{
#if SX_PLATFORM_ANDROID
    the_vfs->mount_android_assets("/assets");
#else
    // mount `/asset` directory
    char asset_dir[RIZZ_MAX_PATH];
    sx_os_path_join(asset_dir, sizeof(asset_dir), EXAMPLES_ROOT, "assets");    // "/examples/assets"
    the_vfs->mount(asset_dir, "/assets");
#endif

    sx_rng_seed_time(&g_draw3d.rng);

    // register main graphics stage.
    // at least one stage should be registered if you want to draw anything
    g_draw3d.stage = the_gfx->stage_register("main", (rizz_gfx_stage){ .id = 0 });
    sx_assert(g_draw3d.stage.id);

    // camera
    // projection: setup for perspective
    // view: Z-UP Y-Forward (like blender)
    sx_vec2 screen_size;
    the_app->window_size(&screen_size);
    const float view_width = screen_size.x;
    const float view_height = screen_size.y;
    the_camera->init(&g_draw3d.cam, 60.0f, sx_rectwh(0, 0, view_width, view_height), 0.1f, 500.0f);
    the_camera->lookat(&g_draw3d.cam, sx_vec3f(10, 10, 20), SX_VEC3_ZERO, SX_VEC3_UNITZ);

    {    // load cube model
        const char* path = "/assets/models/cube.glb";
        g_draw3d.cube_model = the_asset->load("model", path,
                                              &(rizz_model_load_params){
                                                  .layout = k_stream_layout,
                                                  .vbuff_usage = SG_USAGE_IMMUTABLE,
                                                  .ibuff_usage = SG_USAGE_IMMUTABLE,
                                              },
                                              RIZZ_ASSET_LOAD_FLAG_WAIT_ON_LOAD, NULL, 0);
    }

    char shader_path[RIZZ_MAX_PATH];
    g_draw3d.shader = the_asset->load(
        "shader", ex_shader_path(shader_path, sizeof(shader_path), "/assets/shaders", "draw3d.sgs"),
        NULL, 0, NULL, 0);

    sg_pipeline_desc pip_desc = { .layout.buffers[0].stride = sizeof(vertex_stream1),
                                  .layout.buffers[1].stride = sizeof(vertex_stream2),
                                  .layout.buffers[2].stride = sizeof(vertex_stream3),
                                  .index_type = SG_INDEXTYPE_UINT16,
                                  .shader = the_gfx->shader_get(g_draw3d.shader)->shd,
                                  .rasterizer = { .cull_mode = SG_CULLMODE_BACK },
                                  .depth_stencil = { .depth_compare_func =
                                                         SG_COMPAREFUNC_LESS_EQUAL,
                                                     .depth_write_enabled = true } };

    static rizz_vertex_layout k_vertex_layout = {
        .attrs[0] = { .semantic = "POSITION",
                      .offset = offsetof(vertex_stream1, pos),
                      .buffer_index = 0 },
        .attrs[1] = { .semantic = "NORMAL",
                      .offset = offsetof(vertex_stream2, normal),
                      .buffer_index = 1 },
        .attrs[2] = { .semantic = "TEXCOORD",
                      .offset = offsetof(vertex_stream3, uv),
                      .buffer_index = 2 }
    };

    g_draw3d.pip = the_gfx->make_pipeline(the_gfx->shader_bindto_pipeline(
        the_gfx->shader_get(g_draw3d.shader), &pip_desc, &k_vertex_layout));

    g_draw3d.light_dir = sx_vec3f(0, 0.5f, -1.0f);
    g_draw3d.world_mat = sx_mat4_ident();

    sx_color checker_colors[] = { sx_color4u(200, 200, 200, 255), sx_color4u(255, 255, 255, 255) };
    g_draw3d.checker_tex = the_gfx->texture_create_checker(8, 128, checker_colors);

    // cube path
    g_draw3d.cube_path[0] = (rizz_spline3d_node){
        .pos = sx_vec3f(0, 0.1f, 0.5f),
        .lwing = sx_vec3f(-1, 0, 0),
        .rwing = sx_vec3f(1, 0, 0),
    };
    g_draw3d.cube_path[1] = (rizz_spline3d_node){
        .pos = sx_vec3f(2, -0.1f, 0.5f),
        .lwing = sx_vec3f(0, 1, 0),
        .rwing = sx_vec3f(0, -1, 0),
    };
    g_draw3d.cube_path[2] = (rizz_spline3d_node){
        .pos = sx_vec3f(0, -1, 0.5f),
        .lwing = sx_vec3f(1, 0, 0),
        .rwing = sx_vec3f(-1, 0, 0),
    };
    g_draw3d.cube_path[3] = (rizz_spline3d_node){
        .pos = sx_vec3f(-2, -0.1f, 0.5f),
        .lwing = sx_vec3f(0, -2, 0),
        .rwing = sx_vec3f(0, 2, 0),
    };

    // camera path
    g_draw3d.cam_path[0] = (cam_spline_node){
        .base.pos = sx_vec3f(0, 4, 8),
        .base.lwing = sx_vec3f(-4, 0, 0),
        .base.rwing = sx_vec3f(4, 0, 0),
        .fov = 60,
    };
    g_draw3d.cam_path[0].rot = quat_lookat(g_draw3d.cam_path[0].base.pos, SX_VEC3_ZERO);

    g_draw3d.cam_path[1] = (cam_spline_node){
        .base.pos = sx_vec3f(4, 0, 8),
        .base.lwing = sx_vec3f(0, 4, 0),
        .base.rwing = sx_vec3f(0, -4, 0),
        .fov = 30,
    };
    g_draw3d.cam_path[1].rot = quat_lookat(g_draw3d.cam_path[1].base.pos, SX_VEC3_ZERO);

    g_draw3d.cam_path[2] = (cam_spline_node){
        .base.pos = sx_vec3f(0, -4, 8),
        .base.lwing = sx_vec3f(4, 0, 0),
        .base.rwing = sx_vec3f(-4, 0, 0),
        .fov = 60,
    };
    g_draw3d.cam_path[2].rot = quat_lookat(g_draw3d.cam_path[2].base.pos, SX_VEC3_ZERO);

    g_draw3d.cam_path[3] = (cam_spline_node){
        .base.pos = sx_vec3f(-4, 0, 8),
        .base.lwing = sx_vec3f(0, -4, 0),
        .base.rwing = sx_vec3f(0, 4, 0),
        .fov = 120,
    };
    g_draw3d.cam_path[3].rot = quat_lookat(g_draw3d.cam_path[3].base.pos, SX_VEC3_ZERO);

    return true;
}

static void shutdown(void)
{
    if (g_draw3d.shader.id) {
        the_asset->unload(g_draw3d.shader);
    }

    if (g_draw3d.cube_model.id)
        the_asset->unload(g_draw3d.cube_model);
}

static void update(float dt)
{
    float t = (float)sx_tm_sec(the_core->elapsed_tick());

    the_utility->spline.eval3d(
        &(rizz_spline3d_desc){
            .nodes = g_draw3d.cube_path,
            .num_nodes = 4,
            .loop = true,
            .norm = false,
            .time = t,
        },
        &g_draw3d.cube_pos);

    the_utility->spline.eval3d(
        &(rizz_spline3d_desc){
            .nodes = (rizz_spline3d_node*)g_draw3d.cam_path,
            .num_nodes = 4,
            .node_stride = sizeof(cam_spline_node),
            .loop = true,
            .norm = false,
            .time = t * 0.4f,
            .usereval = cam_spline_eval,
        },
        (sx_vec3*)(&g_draw3d.cam_evalresult));

    {    // update camera

        g_draw3d.cam.pos = g_draw3d.cam_evalresult.base.pos;
        g_draw3d.cam.quat = g_draw3d.cam_evalresult.rot;
        static float freq = 2.0f;
        static float gain = 2.0f;
        // static float freq2 = 10.0f;
        // static float gain2 = 0.25f;
        sx_quat shake = sx_quat_fromeular(
            (sx_vec3){ .x = (the_utility->noise.perlin1d(freq * (t + 000)) * sx_torad(gain)),
                       .z = (the_utility->noise.perlin1d(freq * (t + 100)) * sx_torad(gain)) });
        g_draw3d.cam.quat = sx_quat_mul(g_draw3d.cam.quat, shake);
        sx_mat3 m = sx_quat_mat3(g_draw3d.cam.quat);
        g_draw3d.cam.right = sx_vec3fv(m.fc1);
        g_draw3d.cam.forward = sx_vec3fv(m.fc2);
        g_draw3d.cam.up = sx_vec3fv(m.fc3);
        g_draw3d.cam.fov = g_draw3d.cam_evalresult.fov;
    }

    // imgui
    show_debugmenu(the_imgui, the_core);
}

static void render(void)
{
    sg_pass_action pass_action = {
        .colors[0] = { SG_ACTION_CLEAR, { 0.25f, 0.5f, 0.75f, 1.0f } },
        .depth = { SG_ACTION_CLEAR, 1.0f },
    };
    the_gfx->staged.begin(g_draw3d.stage);
    the_gfx->staged.begin_default_pass(&pass_action, the_app->width(), the_app->height());

    sx_mat4 proj, view;
    the_camera->perspective_mat(&g_draw3d.cam, &proj);
    the_camera->view_mat(&g_draw3d.cam, &view);
    sx_mat4 viewproj = sx_mat4_mul(&proj, &view);

    the_3d->debug.grid_xyplane_cam(1.0f, 5.0f, 50.0f, &g_draw3d.cam, &viewproj);

    // draw cube spline
    sx_vec3 points[100];
    for (int i = 0; i < 100; i++) {
        the_utility->spline.eval3d(
            &(rizz_spline3d_desc){
                .nodes = g_draw3d.cube_path,
                .num_nodes = 4,
                .loop = true,
                .norm = true,
                .time = (float)(i / 99.0f),
            },
            &points[i]);
    }
    the_3d->debug.draw_path(points, 100, &viewproj, SX_COLOR_YELLOW);

    // model
    const rizz_model* model = the_3d->model.get(g_draw3d.cube_model);
    draw3d_vertex_shader_uniforms vs_uniforms = { .viewproj_mat = viewproj };
    draw3d_fragment_shader_uniforms fs_uniforms = { .light_dir = sx_vec3_norm(g_draw3d.light_dir) };
    the_gfx->staged.apply_pipeline(g_draw3d.pip);

    const rizz_model_mesh* mesh = &model->meshes[0];
    sx_tx3d xform = sx_tx3d_set(g_draw3d.cube_pos, sx_mat3_ident());
    sx_mat4 node_mat = sx_tx3d_mat4(&xform);
    vs_uniforms.world_mat = sx_mat4_mul(&g_draw3d.world_mat, &node_mat);
    the_gfx->staged.apply_uniforms(SG_SHADERSTAGE_VS, 0, &vs_uniforms, sizeof(vs_uniforms));

    sg_bindings bind = {
        .vertex_buffers[0] = mesh->gpu.vbuffs[0],
        .vertex_buffers[1] = mesh->gpu.vbuffs[1],
        .vertex_buffers[2] = mesh->gpu.vbuffs[2],
        .index_buffer = mesh->gpu.ibuff,
        .fs_images[0] = g_draw3d.checker_tex.img,
    };
    for (int mi = 0; mi < mesh->num_submeshes; mi++) {
        const rizz_model_submesh* submesh = &mesh->submeshes[mi];
        bind.index_buffer_offset = submesh->start_index * sizeof(uint16_t);
        fs_uniforms.color = sx_vec3f(1, 1, 1);
        the_gfx->staged.apply_uniforms(SG_SHADERSTAGE_FS, 0, &fs_uniforms, sizeof(fs_uniforms));
        the_gfx->staged.apply_bindings(&bind);
        the_gfx->staged.draw(0, submesh->num_indices, 1);
    }

    the_gfx->staged.end_pass();
    the_gfx->staged.end();
}

rizz_plugin_decl_main(draw3d, plugin, e)
{
    switch (e) {
    case RIZZ_PLUGIN_EVENT_STEP:
        update(the_core->delta_time());
        render();
        break;

    case RIZZ_PLUGIN_EVENT_INIT:
        // runs only once for application. Retreive needed APIs
        the_core = (rizz_api_core*)plugin->api->get_api(RIZZ_API_CORE, 0);
        the_gfx = (rizz_api_gfx*)plugin->api->get_api(RIZZ_API_GFX, 0);
        the_app = (rizz_api_app*)plugin->api->get_api(RIZZ_API_APP, 0);
        the_vfs = (rizz_api_vfs*)plugin->api->get_api(RIZZ_API_VFS, 0);
        the_asset = (rizz_api_asset*)plugin->api->get_api(RIZZ_API_ASSET, 0);
        the_camera = (rizz_api_camera*)plugin->api->get_api(RIZZ_API_CAMERA, 0);
        the_imgui = (rizz_api_imgui*)plugin->api->get_api_byname("imgui", 0);
        the_imguix = (rizz_api_imgui_extra*)plugin->api->get_api_byname("imgui_extra", 0);
        the_3d = (rizz_api_3d*)plugin->api->get_api_byname("3dtools", 0);
        the_utility = (rizz_api_utility*)plugin->api->get_api_byname("utility", 0);

        init();
        break;

    case RIZZ_PLUGIN_EVENT_LOAD:
        break;

    case RIZZ_PLUGIN_EVENT_UNLOAD:
        break;

    case RIZZ_PLUGIN_EVENT_SHUTDOWN:
        shutdown();
        break;
    }

    return 0;
}

rizz_game_decl_config(conf)
{
    conf->app_name = "";
    conf->app_version = 1000;
    conf->app_title = "spline";
    conf->app_flags |= RIZZ_APP_FLAG_HIGHDPI;
    conf->log_level = RIZZ_LOG_LEVEL_DEBUG;
    conf->window_width = EXAMPLES_DEFAULT_WIDTH;
    conf->window_height = EXAMPLES_DEFAULT_HEIGHT;
    conf->multisample_count = 4;
    conf->swap_interval = 1;
    conf->plugins[0] = "imgui";
    conf->plugins[1] = "3dtools";
    conf->plugins[2] = "utility";
}