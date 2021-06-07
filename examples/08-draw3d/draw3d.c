#include "rizz/config.h"
#include "sx/allocator.h"
#include "sx/io.h"
#include "sx/math-vec.h"
#include "sx/os.h"
#include "sx/string.h"
#include "sx/rng.h"

#include "rizz/imgui.h"
#include "rizz/imgui-extra.h"
#include "rizz/3dtools.h"
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

#define NUM_MODELS 3
static const char* k_models[NUM_MODELS] = {
    "torus.glb",
    "cube_multipart.glb",
    "monkey_nodes.glb"
};

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

typedef enum draw3d_gizmo_type {
    GIZMO_TYPE_TRANSLATE = 0,
    GIZMO_TYPE_ROTATE,
    GIZMO_TYPE_SCALE
} draw3d_gizmo_type;

typedef struct {
    sx_rng rng;
    rizz_gfx_stage stage;
    rizz_camera_fps cam;
    rizz_asset models[NUM_MODELS];
    rizz_asset shader;
    sg_pipeline pip;
    sx_vec3 light_dir;
    draw3d_gizmo_type gizmo_type;
    sx_mat4 world_mat;
    rizz_texture checker_tex;
    int model_index;
    bool show_grid;
    bool show_debug_cubes;
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
};

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
    the_camera->fps_init(&g_draw3d.cam, 50.0f, sx_rectwh(0, 0, view_width, view_height), 0.1f, 500.0f);
    the_camera->fps_lookat(&g_draw3d.cam, sx_vec3f(0, -3.0f, 3.0f), SX_VEC3_ZERO, SX_VEC3_UNITZ);

    for (int i = 0; i < NUM_MODELS; i++) {
        rizz_log_info("loading model: %s", k_models[i]);
        char filepath[RIZZ_MAX_PATH];
        sx_strcat(sx_strcpy(filepath, sizeof(filepath), "/assets/models/"), sizeof(filepath), k_models[i]);
        g_draw3d.models[i] = the_asset->load("model", filepath, &(rizz_model_load_params) {
            .layout = k_stream_layout, 
            .vbuff_usage = SG_USAGE_IMMUTABLE, 
            .ibuff_usage = SG_USAGE_IMMUTABLE
        }, 0, NULL, 0);
        sx_assert_always(g_draw3d.models[i].id);
    }

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
        .rasterizer = { .cull_mode = SG_CULLMODE_BACK },
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
    g_draw3d.show_grid = true;

    return true;
}

static void shutdown(void)
{
    if (g_draw3d.shader.id) {
        the_asset->unload(g_draw3d.shader);
    }
    for (int i = 0; i < NUM_MODELS; i++) {
        if (g_draw3d.models[i].id) {
            the_asset->unload(g_draw3d.models[i]);
        }
    }
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
    show_debugmenu(the_imgui, the_core);
    
    if (the_imgui->Begin("draw3d", NULL, 0)) {
        the_imgui->TextWrapped("Use WASD/Arrow keys to move, Hold left mouse button to look around");
        the_imgui->SliderFloat3("light_dir", g_draw3d.light_dir.f, -1.0f, 1.0f, "%.2f", 0);
        if (sx_equal(sx_vec3_len(g_draw3d.light_dir), 0, 0.0001f)) {
            g_draw3d.light_dir.y = 0.1f;
        }

        if (the_imgui->RadioButton_Bool("Translate", g_draw3d.gizmo_type == GIZMO_TYPE_TRANSLATE)) {
            g_draw3d.gizmo_type = GIZMO_TYPE_TRANSLATE;
        }
        the_imgui->SameLine(0, -1);
        if (the_imgui->RadioButton_Bool("Rotate", g_draw3d.gizmo_type == GIZMO_TYPE_ROTATE)) {
            g_draw3d.gizmo_type = GIZMO_TYPE_ROTATE;
        }
        the_imgui->SameLine(0, -1);
        if (the_imgui->RadioButton_Bool("Scale", g_draw3d.gizmo_type == GIZMO_TYPE_SCALE)) {
            g_draw3d.gizmo_type = GIZMO_TYPE_SCALE;
        }

        if (the_imgui->Button("Reset Transform", SX_VEC2_ZERO)) {
            g_draw3d.world_mat = sx_mat4_ident();
        }

        if (the_imgui->BeginCombo("Model", k_models[g_draw3d.model_index], 0)) {
            for (int i = 0; i < 3; i++) {
                bool selected = g_draw3d.model_index == i;
                if (the_imgui->Selectable_Bool(k_models[i], selected, 0, SX_VEC2_ZERO)) {
                    g_draw3d.model_index = i;
                }
                if (selected) {
                    the_imgui->SetItemDefaultFocus();
                }
            }
            the_imgui->EndCombo();    
        }

        the_imgui->Checkbox("Show grid", &g_draw3d.show_grid);
        the_imgui->Checkbox("Show debug cubes", &g_draw3d.show_debug_cubes);
        
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

    sx_mat4 proj, view;
    the_camera->perspective_mat(&g_draw3d.cam.cam, &proj);
    the_camera->view_mat(&g_draw3d.cam.cam, &view);
    sx_mat4 viewproj = sx_mat4_mul(&proj, &view);

    // Initialize and cache 100 debug boxes
    static sx_box boxes[100];
    static sx_color tints[100];
    static bool init_boxes = false;
    if (!init_boxes) {
        for (int i = 0; i < 100; i++) {
            boxes[i] = sx_box_set(sx_tx3d_setf(
                sx_rng_gen_rangef(&g_draw3d.rng, -50.0f, 50.0f), 
                sx_rng_gen_rangef(&g_draw3d.rng, -50.0f, 50.0f), 
                0, 
                0, 0, sx_torad(sx_rng_gen_rangef(&g_draw3d.rng, 0, 90.0f))), 
                sx_vec3f(sx_rng_genf(&g_draw3d.rng) + 0.5f, sx_rng_genf(&g_draw3d.rng) + 0.5f, 0.5f));
            tints[i] = sx_color4u(sx_rng_gen_rangei(&g_draw3d.rng, 0, 255),
                                  sx_rng_gen_rangei(&g_draw3d.rng, 0, 255),
                                  sx_rng_gen_rangei(&g_draw3d.rng, 0, 255), 
                                  255);
        }
        init_boxes = true;
    }
    
    if (g_draw3d.show_grid) {
        the_3d->debug.grid_xyplane_cam(1.0f, 5.0f, 50.0f, &g_draw3d.cam.cam, &viewproj);
    }

    if (g_draw3d.show_debug_cubes) {
        the_3d->debug.draw_boxes(boxes, 100, &viewproj, RIZZ_3D_DEBUG_MAPTYPE_CHECKER, tints);
    }

    // model

    const rizz_model* model = the_3d->model.get(g_draw3d.models[g_draw3d.model_index]);
    draw3d_vertex_shader_uniforms vs_uniforms = { .viewproj_mat = viewproj };
    draw3d_fragment_shader_uniforms fs_uniforms = { .light_dir = sx_vec3_norm(g_draw3d.light_dir) };
    the_gfx->staged.apply_pipeline(g_draw3d.pip);
    

    sx_aabb* bounds = alloca(sizeof(sx_aabb)*model->num_nodes);
    int num_bounds = 0;
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
            if (submesh->mtl.id) {
                fs_uniforms.color = sx_vec3fv(
                    the_3d->material.get_data(model->mtllib, submesh->mtl)->pbr_metallic_roughness.base_color_factor.f);
            } else {
                fs_uniforms.color = sx_vec3f(1.0f, 1.0f, 1.0f);
            }
            the_gfx->staged.apply_uniforms(SG_SHADERSTAGE_FS, 0, &fs_uniforms, sizeof(fs_uniforms));
            the_gfx->staged.apply_bindings(&bind);
            the_gfx->staged.draw(0, submesh->num_indices, 1);
        }

        bounds[num_bounds++] = sx_aabb_transform(&node->bounds, &vs_uniforms.world_mat);
    }

    if (num_bounds > 0) {
        the_3d->debug.draw_aabbs(bounds, num_bounds, &viewproj, NULL);
    }

    the_gfx->staged.end_pass();
    the_gfx->staged.end();

    switch (g_draw3d.gizmo_type) {
    case GIZMO_TYPE_TRANSLATE:  the_imguix->gizmo.translate(&g_draw3d.world_mat, &view, &proj, GIZMO_MODE_WORLD, NULL, NULL); break;
    case GIZMO_TYPE_ROTATE:     the_imguix->gizmo.rotation(&g_draw3d.world_mat, &view, &proj, GIZMO_MODE_WORLD, NULL, NULL); break;
    case GIZMO_TYPE_SCALE:      the_imguix->gizmo.scale(&g_draw3d.world_mat, &view, &proj, GIZMO_MODE_WORLD, NULL, NULL); break;
    }
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

rizz_plugin_decl_event_handler(draw3d, e)
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

            if (!the_imguix->gizmo.is_using() && !the_imguix->is_capturing_mouse()) {
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
    conf->app_name = "draw3d";
    conf->app_version = 1000;
    conf->app_title = "draw3d";
    conf->app_flags |= RIZZ_APP_FLAG_HIGHDPI;
    conf->log_level = RIZZ_LOG_LEVEL_DEBUG;
    conf->window_width = EXAMPLES_DEFAULT_WIDTH;
    conf->window_height = EXAMPLES_DEFAULT_HEIGHT;
    conf->multisample_count = 4;
    conf->swap_interval = 1;
    conf->plugins[0] = "imgui";
    conf->plugins[1] = "3dtools";
}