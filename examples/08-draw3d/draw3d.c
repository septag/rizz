#include "sx/allocator.h"
#include "sx/io.h"
#include "sx/math.h"
#include "sx/os.h"
#include "sx/string.h"

#include "rizz/imgui.h"
#include "rizz/imgui-extra.h"
#include "rizz/3dtools.h"
#include "rizz/rizz.h"

#include "../common.h"

RIZZ_STATE static rizz_api_core* the_core;
RIZZ_STATE static rizz_api_gfx* the_gfx;
RIZZ_STATE static rizz_api_app* the_app;
RIZZ_STATE static rizz_api_imgui* the_imgui;
RIZZ_STATE static rizz_api_asset* the_asset;
RIZZ_STATE static rizz_api_camera* the_camera;
RIZZ_STATE static rizz_api_vfs* the_vfs;
RIZZ_STATE static rizz_api_prims3d* the_prims;
RIZZ_STATE static rizz_api_imgui_extra* the_imguix;

typedef struct rizz_model rizz_model;

typedef struct {
    sx_mat4 viewproj_mat;
    sx_mat4 world_mat;
} draw3d_vertex_shader_uniforms;

typedef struct {
    sx_vec3 light_dir;
} draw3d_fragment_shader_uniforms;

typedef enum draw3d_gizmo_type {
    GIZMO_TYPE_TRANSLATE = 0,
    GIZMO_TYPE_ROTATE,
    GIZMO_TYPE_SCALE
} draw3d_gizmo_type;

typedef struct {
    rizz_gfx_stage stage;
    rizz_camera_fps cam;
    rizz_model* model;
    rizz_asset shader;
    sg_pipeline pip;
    sx_vec3 light_dir;
    draw3d_gizmo_type gizmo_type;
    sx_mat4 world_mat;
    rizz_texture checker_tex;
} draw3d_state;

RIZZ_STATE static draw3d_state g_draw3d;

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

    // register main graphics stage.
    // at least one stage should be registered if you want to draw anything
    g_draw3d.stage = the_gfx->stage_register("main", (rizz_gfx_stage){ .id = 0 });
    sx_assert(g_draw3d.stage.id);

    // camera
    // projection: setup for perspective
    // view: Z-UP Y-Forward (like blender)
    sx_vec2 screen_size = the_app->sizef();
    const float view_width = screen_size.x;
    const float view_height = screen_size.y;
    the_camera->fps_init(&g_draw3d.cam, 50.0f, sx_rectwh(0, 0, view_width, view_height), 0.1f, 500.0f);
    the_camera->fps_lookat(&g_draw3d.cam, sx_vec3f(0, -3.0f, 3.0f), SX_VEC3_ZERO, SX_VEC3_UNITZ);

    load_model("/assets/models/monkey.glb", &g_draw3d.model, &(rizz_model_geometry_layout) {
                .attrs[0] = { .semantic="POSITION", .offset=offsetof(vertex_stream1, pos), 
                              .buffer_index=0, .format=SG_VERTEXFORMAT_FLOAT3 },
                .attrs[1] = { .semantic="NORMAL", .offset=offsetof(vertex_stream2, normal), 
                              .buffer_index=1, .format=SG_VERTEXFORMAT_FLOAT3 },
                .attrs[2] = { .semantic="TANGENT", .offset=offsetof(vertex_stream2, tangent), 
                              .buffer_index=1, .format=SG_VERTEXFORMAT_FLOAT3 },
                .attrs[3] = { .semantic="BINORMAL", .offset=offsetof(vertex_stream2, bitangent), 
                              .buffer_index=1, .format=SG_VERTEXFORMAT_FLOAT3 },
                .attrs[4] = { .semantic="TEXCOORD", .offset=offsetof(vertex_stream3, uv), 
                              .buffer_index=2, .format=SG_VERTEXFORMAT_FLOAT2 },
                .attrs[5] = { .semantic="COLOR", .offset=offsetof(vertex_stream3, color), 
                              .buffer_index=2, .format=SG_VERTEXFORMAT_UBYTE4N },
                .buffer_strides[0] = sizeof(vertex_stream1),
                .buffer_strides[1] = sizeof(vertex_stream2),
                .buffer_strides[2] = sizeof(vertex_stream3)
            });
    sx_assert(g_draw3d.model);

    char shader_path[RIZZ_MAX_PATH];
    g_draw3d.shader = the_asset->load("shader", 
        ex_shader_path(shader_path, sizeof(shader_path), "/assets/shaders", "draw3d.sgs"), 
        NULL, 0, NULL, 0);

    sg_pipeline_desc pip_desc = { 
        .layout.buffers[0].stride = sizeof(vertex_stream1),
        .layout.buffers[1].stride = sizeof(vertex_stream2),
        .layout.buffers[2].stride = sizeof(vertex_stream3),
        .index_type = SG_INDEXTYPE_UINT16,
        .shader = the_gfx->shader_get(g_draw3d.shader)->shd,
        .rasterizer = { .cull_mode = SG_CULLMODE_FRONT },
        .depth_stencil = { .depth_compare_func = SG_COMPAREFUNC_LESS_EQUAL, .depth_write_enabled = true}
    };

    static rizz_vertex_layout k_vertex_layout = {
        .attrs[0] = { .semantic = "POSITION", .offset = offsetof(vertex_stream1, pos), .buffer_index = 0 },
        .attrs[1] = { .semantic = "NORMAL", .offset = offsetof(vertex_stream2, normal), .buffer_index = 1},
        .attrs[2] = { .semantic = "TEXCOORD", .offset = offsetof(vertex_stream3, uv), .buffer_index = 2 }
    };

    g_draw3d.pip = the_gfx->make_pipeline(
        the_gfx->shader_bindto_pipeline(the_gfx->shader_get(g_draw3d.shader), &pip_desc, &k_vertex_layout));

    g_draw3d.light_dir = sx_vec3f(0, 0.5f, -1.0f);
    g_draw3d.world_mat = sx_mat4_ident();

    sx_color checker_colors[] = {
        sx_color4u(200, 200, 200, 255), 
        sx_color4u(255, 255, 255, 255)
    };
    g_draw3d.checker_tex = the_gfx->texture_create_checker(8, 128, checker_colors);
    
    return true;
}

static void shutdown(void)
{
}

static void update(float dt)
{
    const float move_speed = 3.0f;

    // update keyboard movement: WASD first-person
    if (the_app->key_pressed(RIZZ_APP_KEYCODE_A) || the_app->key_pressed(RIZZ_APP_KEYCODE_LEFT)) {
        the_camera->fps_strafe(&g_draw3d.cam, -move_speed*dt);
    }

    if (the_app->key_pressed(RIZZ_APP_KEYCODE_D) || the_app->key_pressed(RIZZ_APP_KEYCODE_RIGHT)) {
        the_camera->fps_strafe(&g_draw3d.cam, move_speed*dt);
    }

    if (the_app->key_pressed(RIZZ_APP_KEYCODE_W) || the_app->key_pressed(RIZZ_APP_KEYCODE_UP)) {
        the_camera->fps_forward(&g_draw3d.cam, move_speed*dt);
    }

    if (the_app->key_pressed(RIZZ_APP_KEYCODE_S) || the_app->key_pressed(RIZZ_APP_KEYCODE_DOWN)) {
        the_camera->fps_forward(&g_draw3d.cam, -move_speed*dt);
    }

    // imgui
    if (the_imgui->Begin("draw3d", NULL, 0)) {
        the_imgui->SliderFloat3("light_dir", g_draw3d.light_dir.f, -1.0f, 1.0f, "%.2f", 1.0f);
        if (sx_equal(sx_vec3_len(g_draw3d.light_dir), 0, 0.0001f)) {
            g_draw3d.light_dir.y = 0.1f;
        }

        if (the_imgui->RadioButtonBool("Translate", g_draw3d.gizmo_type == GIZMO_TYPE_TRANSLATE)) {
            g_draw3d.gizmo_type = GIZMO_TYPE_TRANSLATE;
        }
        the_imgui->SameLine(0, -1);
        if (the_imgui->RadioButtonBool("Rotate", g_draw3d.gizmo_type == GIZMO_TYPE_ROTATE)) {
            g_draw3d.gizmo_type = GIZMO_TYPE_ROTATE;
        }
        the_imgui->SameLine(0, -1);
        if (the_imgui->RadioButtonBool("Scale", g_draw3d.gizmo_type == GIZMO_TYPE_SCALE)) {
            g_draw3d.gizmo_type = GIZMO_TYPE_SCALE;
        }

        if (the_imgui->Button("Reset", SX_VEC2_ZERO)) {
            g_draw3d.world_mat = sx_mat4_ident();
        }
    }
    the_imgui->End();
}

static sx_tx3d model__calc_transform(const rizz_model* model, const rizz_model_node* node)
{
    sx_tx3d tx = node->local_tx;
    int parent_id = node->parent_id;
    while (parent_id != -1) {
        tx = sx_tx3d_mul(&model->nodes[parent_id].local_tx, &tx);
        parent_id = model->nodes[parent_id].parent_id;
    }
    return tx;
}

static void render(void) 
{
    sg_pass_action pass_action = { .colors[0] = { SG_ACTION_CLEAR, {0.25f, 0.5f, 0.75f, 1.0f} },
                                   .depth = { SG_ACTION_CLEAR, 1.0f } };
    the_gfx->staged.begin(g_draw3d.stage);
    the_gfx->staged.begin_default_pass(&pass_action, the_app->width(), the_app->height());

    sx_mat4 proj = the_camera->perspective_mat(&g_draw3d.cam.cam);
    sx_mat4 view = the_camera->view_mat(&g_draw3d.cam.cam);
    sx_mat4 viewproj = sx_mat4_mul(&proj, &view);
    static sx_box boxes[100];
    static sx_color tints[100];
    static bool init_boxes = false;
    if (!init_boxes) {
        for (int i = 0; i < 100; i++) {
            boxes[i] = sx_box_set(sx_tx3d_setf(
                the_core->rand_range(-50.0f, 50.0f), the_core->rand_range(-50.0f, 50.0f), 0, 
                0, 0, sx_torad(the_core->rand_range(0, 90.0f))), 
                sx_vec3f(the_core->rand_range(1.0f, 2.0f), the_core->rand_range(1.0f, 2.0), 1.0f)); 
            tints[i] = sx_color4u(the_core->rand_range(0, 255), the_core->rand_range(0, 255), the_core->rand_range(0, 255), 255);
        }
        init_boxes = true;
    }
    
    the_prims->grid_xyplane_cam(1.0f, 5.0f, 50.0f, &g_draw3d.cam.cam, &viewproj);

    #if 0
    the_prims->draw_boxes(boxes, 100, &viewproj, RIZZ_PRIMS3D_MAPTYPE_CHECKER, tints);


    sx_aabb aabb = sx_aabbwhd(1.0f, 2.0f, 0.5f);
    sx_vec3 pos = sx_vec3fv(mat.col4.f);
    aabb = sx_aabb_setpos(&aabb, pos);
    the_prims->draw_aabb(&aabb, &viewproj, SX_COLOR_WHITE);
    #endif

    // model

    const rizz_model* model = g_draw3d.model;
    draw3d_vertex_shader_uniforms vs_uniforms = { .viewproj_mat = viewproj };
    draw3d_fragment_shader_uniforms fs_uniforms = { .light_dir = sx_vec3_norm(g_draw3d.light_dir) };
    the_gfx->staged.apply_pipeline(g_draw3d.pip);
    the_gfx->staged.apply_uniforms(SG_SHADERSTAGE_FS, 0, &fs_uniforms, sizeof(fs_uniforms));

    for (int i = 0; i < model->num_nodes; i++) {
        const rizz_model_node* node = &model->nodes[i];
        if (node->mesh_id == -1) {
            continue;
        }

        sx_tx3d tx = model__calc_transform(model, node);
        sx_mat4 node_mat = sx_tx3d_mat4(&tx);
        vs_uniforms.world_mat = sx_mat4_mul(&g_draw3d.world_mat, &node_mat);
        the_gfx->staged.apply_uniforms(SG_SHADERSTAGE_VS, 0, &vs_uniforms, sizeof(vs_uniforms));

        const rizz_model_mesh* mesh = &model->meshes[node->mesh_id];
        sg_bindings bind = {
            .vertex_buffers[0] = mesh->gpu.vbuffs[0],
            .vertex_buffers[1] = mesh->gpu.vbuffs[1],
            .vertex_buffers[2] = mesh->gpu.vbuffs[2],
            .index_buffer = mesh->gpu.ibuff,
            .fs_images[0] = g_draw3d.checker_tex.img
        };
        for (int mi = 0; mi < mesh->num_submeshes; mi++) {
            const rizz_model_submesh* submesh = &mesh->submeshes[mi];
            bind.index_buffer_offset = submesh->start_index * sizeof(uint16_t);
            the_gfx->staged.apply_bindings(&bind);
            the_gfx->staged.draw(0, submesh->num_indices, 1);
        }
    }

    sx_aabb aabb = sx_aabb_transform(&model->bounds, &g_draw3d.world_mat);
    the_prims->draw_aabb(&aabb, &viewproj, SX_COLOR_WHITE);

    the_gfx->staged.end_pass();
    the_gfx->staged.end();

    switch (g_draw3d.gizmo_type) {
    case GIZMO_TYPE_TRANSLATE:  the_imguix->gizmo_translate(&g_draw3d.world_mat, &view, &proj, GIZMO_MODE_WORLD, NULL, NULL); break;
    case GIZMO_TYPE_ROTATE:     the_imguix->gizmo_rotation(&g_draw3d.world_mat, &view, &proj, GIZMO_MODE_WORLD, NULL, NULL); break;
    case GIZMO_TYPE_SCALE:      the_imguix->gizmo_scale(&g_draw3d.world_mat, &view, &proj, GIZMO_MODE_WORLD, NULL, NULL); break;
    }
}

rizz_plugin_decl_main(draw3d, plugin, e)
{
    switch (e) {
    case RIZZ_PLUGIN_EVENT_STEP:
        update((float)sx_tm_sec(the_core->delta_tick()));
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
        the_prims = (rizz_api_prims3d*)plugin->api->get_api_byname("prims3d", 0);

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

rizz_plugin_decl_event_handler(nbody, e)
{
    static bool mouse_down = false;
    float dt = (float)sx_tm_sec(the_core->delta_tick());
    const float rotate_speed = 5.0f;
    static sx_vec2 last_mouse;

    switch (e->type) {
    case RIZZ_APP_EVENTTYPE_SUSPENDED:
        break;
    case RIZZ_APP_EVENTTYPE_RESTORED:
        break;
    case RIZZ_APP_EVENTTYPE_MOUSE_DOWN:
        if (!mouse_down) {
            the_app->mouse_capture();
        }
        mouse_down = true;
        last_mouse = sx_vec2f(e->mouse_x, e->mouse_y);
        break;
    case RIZZ_APP_EVENTTYPE_MOUSE_UP:
        if (mouse_down) {
            the_app->mouse_release();
        }
    case RIZZ_APP_EVENTTYPE_MOUSE_LEAVE:
        mouse_down = false;
        break;
    case RIZZ_APP_EVENTTYPE_MOUSE_MOVE:
        if (mouse_down) {
            float dx = sx_torad(e->mouse_x - last_mouse.x) * rotate_speed * dt;
            float dy = sx_torad(e->mouse_y - last_mouse.y) * rotate_speed * dt;
            last_mouse = sx_vec2f(e->mouse_x, e->mouse_y);

            if (!the_imguix->gizmo_using() && !the_imguix->is_capturing_mouse()) {
                the_camera->fps_pitch(&g_draw3d.cam, dy);
                the_camera->fps_yaw(&g_draw3d.cam, dx);
            }
        }
        break;
    default:
        break;
    }
}

rizz_game_decl_config(conf)
{
    conf->app_name = "";
    conf->app_version = 1000;
    conf->app_title = "draw3d";
    conf->app_flags |= RIZZ_APP_FLAG_HIGHDPI;
    conf->log_level = RIZZ_LOG_LEVEL_DEBUG;
    conf->window_width = 800;
    conf->window_height = 600;
    conf->multisample_count = 4;
    conf->swap_interval = 1;
    conf->plugins[0] = "imgui";
    conf->plugins[1] = "3dtools";
}