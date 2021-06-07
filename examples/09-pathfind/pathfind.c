#include "rizz/3dtools.h"
#include "rizz/astar.h"
#include "rizz/config.h"
#include "rizz/imgui-extra.h"
#include "rizz/imgui.h"
#include "rizz/rizz.h"

#include "sx/allocator.h"
#include "sx/array.h"
#include "sx/io.h"
#include "sx/math-vec.h"
#include "sx/os.h"
#include "sx/rng.h"
#include "sx/string.h"

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
RIZZ_STATE static rizz_api_astar* the_astar;

#define GRID_WIDTH 16
#define GRID_HEIGHT 16
uint8_t k_celldata[GRID_WIDTH * GRID_HEIGHT] = {
    // clang-format off
    1,1,1,1,1,1,1,0,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,0,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,0,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,0,0,1,0,0,1,1,1,1,1,1,
    1,1,1,1,1,0,0,1,0,0,1,1,1,1,1,1,
    0,1,0,0,1,1,1,1,1,1,1,0,0,1,0,0,
    1,1,1,1,1,0,0,1,0,0,1,1,1,1,1,1,
    1,1,1,1,1,0,0,1,0,0,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,0,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,0,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,0,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,0,1,1,1,1,1,1,1,1,
    // clang-format on
};

uint8_t k_celldata2[GRID_WIDTH * GRID_HEIGHT] = {
    // clang-format off
    1,1,1,1,1,1,1,0,1,1,1,1,1,1,1,1,
    1,1,1,1,3,3,3,3,3,3,3,1,1,1,1,1,
    1,1,1,1,3,3,3,0,3,3,3,1,1,1,1,1,
    1,1,1,1,1,3,3,0,3,3,1,1,1,1,1,1,
    1,4,4,1,1,1,1,1,1,1,1,1,5,5,1,1,
    1,4,4,4,1,0,0,1,0,0,1,5,5,5,1,1,
    1,4,4,4,1,0,0,1,0,0,1,5,5,5,1,1,
    0,4,0,0,1,1,1,1,1,1,1,0,0,5,0,0,
    1,4,4,4,1,0,0,1,0,0,1,5,5,5,1,1,
    1,4,4,4,1,0,0,1,0,0,1,5,5,5,1,1,
    1,4,4,1,1,1,1,1,1,1,1,1,5,5,1,1,
    1,1,1,1,1,2,2,0,2,2,1,1,1,1,1,1,
    1,1,1,1,2,2,2,0,2,2,2,1,1,1,1,1,
    1,1,1,1,2,2,2,2,2,2,2,1,1,1,1,1,
    1,1,1,1,1,1,1,0,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,0,1,1,1,1,1,1,1,1,
    // clang-format on
};

const sx_color k_celltype_colors[6] = {
    { .r = 000, .g = 000, .b = 000, .a = 255 }, { .r = 255, .g = 255, .b = 255, .a = 255 },
    { .r = 255, .g = 000, .b = 000, .a = 255 }, { .r = 000, .g = 255, .b = 000, .a = 255 },
    { .r = 000, .g = 000, .b = 255, .a = 255 }, { .r = 255, .g = 255, .b = 000, .a = 255 },
};

enum {
    CELLTYPE_BLOCK = 0,
    CELL_TYPE_WHITE,
    CELL_TYPE_RED,
    CELL_TYPE_GREEN,
    CELL_TYPE_BLUE,
    CELL_TYPE_YELLOW,
};

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

typedef enum draw3d_gizmo_type {
    GIZMO_TYPE_TRANSLATE = 0,
    GIZMO_TYPE_ROTATE,
    GIZMO_TYPE_SCALE
} draw3d_gizmo_type;

typedef struct {
    sx_tx3d transform;
    sx_color color;
    rizz_astar_agent astar_agent;
    rizz_astar_path path;
    int cindex;
    float waiting;
    bool walking;
} agent_state;

typedef struct {
    sx_rng rng;
    rizz_gfx_stage stage;
    rizz_camera_fps cam;
    rizz_asset agent_model;
    rizz_asset shader;
    rizz_astar_world astar_world;
    agent_state agents[4];
    sg_pipeline pip;
    sx_vec3 light_dir;
    sx_mat4 world_mat;
    rizz_texture checker_tex;
    bool use_grid2;
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

static inline sx_vec2 random_pos()
{
    while (1) {
        int x = sx_rng_gen_rangei(&(g_draw3d.rng), 0, GRID_WIDTH);
        int y = sx_rng_gen_rangei(&(g_draw3d.rng), 0, GRID_HEIGHT);
        if (g_draw3d.astar_world.cells[x + y * GRID_WIDTH] != 0)
            return sx_vec2f((float)x, (float)y);
    }
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
    the_camera->fps_init(&g_draw3d.cam, 50.0f, sx_rectwh(0, 0, view_width, view_height), 0.1f,
                         500.0f);
    the_camera->fps_lookat(&g_draw3d.cam, sx_vec3f(10.0f, -10.0f, 10.0f), sx_vec3f(8, 8, 0),
                           SX_VEC3_UNITZ);


    g_draw3d.agent_model =
        the_asset->load("model", "/assets/models/agent.glb",
                        &(rizz_model_load_params){ .layout = k_stream_layout,
                                                   .vbuff_usage = SG_USAGE_IMMUTABLE,
                                                   .ibuff_usage = SG_USAGE_IMMUTABLE },
                        RIZZ_ASSET_LOAD_FLAG_WAIT_ON_LOAD, NULL, 0);
    sx_assert_always(g_draw3d.agent_model.id);


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

    for (int i = 0; i < 4; i++) {
        g_draw3d.agents[i] = (agent_state){
            .path =
                (rizz_astar_path){
                    .alloc = the_core->alloc(RIZZ_MEMID_GAME),
                    .array = NULL,
                },
            .cindex = 0,
        };
    }

    g_draw3d.astar_world = (rizz_astar_world){
        .width = GRID_WIDTH,
        .height = GRID_HEIGHT,
        .offset = SX_VEC2_ZERO,
        .scale = 1,
        .cells = k_celldata,
    };

    g_draw3d.agents[0].transform = sx_tx3d_setf(00, 00, 00, 00, 00, 00);
    g_draw3d.agents[0].color = SX_COLOR_RED;
    g_draw3d.agents[0].astar_agent = (rizz_astar_agent){
        .costs[CELL_TYPE_WHITE] = 10,
        .costs[CELL_TYPE_RED] = 1,
        .costs[CELL_TYPE_GREEN] = 80,
        .costs[CELL_TYPE_BLUE] = 80,
        .costs[CELL_TYPE_YELLOW] = 80,
    };
    g_draw3d.agents[1].transform = sx_tx3d_setf(15, 00, 00, 00, 00, 00);
    g_draw3d.agents[1].color = SX_COLOR_GREEN;
    g_draw3d.agents[1].astar_agent = (rizz_astar_agent){
        .costs[CELL_TYPE_WHITE] = 10,
        .costs[CELL_TYPE_RED] = 80,
        .costs[CELL_TYPE_GREEN] = 1,
        .costs[CELL_TYPE_BLUE] = 80,
        .costs[CELL_TYPE_YELLOW] = 80,
    };
    g_draw3d.agents[2].transform = sx_tx3d_setf(00, 15, 00, 00, 00, 00);
    g_draw3d.agents[2].color = SX_COLOR_BLUE;
    g_draw3d.agents[2].astar_agent = (rizz_astar_agent){
        .costs[CELL_TYPE_WHITE] = 10,
        .costs[CELL_TYPE_RED] = 80,
        .costs[CELL_TYPE_GREEN] = 80,
        .costs[CELL_TYPE_BLUE] = 1,
        .costs[CELL_TYPE_YELLOW] = 80,
    };
    g_draw3d.agents[3].transform = sx_tx3d_setf(15, 15, 00, 00, 00, 00);
    g_draw3d.agents[3].color = SX_COLOR_YELLOW;
    g_draw3d.agents[3].astar_agent = (rizz_astar_agent){
        .costs[CELL_TYPE_WHITE] = 10,
        .costs[CELL_TYPE_RED] = 80,
        .costs[CELL_TYPE_GREEN] = 80,
        .costs[CELL_TYPE_BLUE] = 80,
        .costs[CELL_TYPE_YELLOW] = 1,
    };
    return true;
}

static void shutdown(void)
{
    if (g_draw3d.shader.id) {
        the_asset->unload(g_draw3d.shader);
    }
    if (g_draw3d.agent_model.id) {
        the_asset->unload(g_draw3d.agent_model);
    }
}

static void update(float dt)
{
    float move_speed = 3.0f;
    if (the_app->key_pressed(RIZZ_APP_KEYCODE_LEFT_SHIFT) ||
        the_app->key_pressed(RIZZ_APP_KEYCODE_RIGHT_SHIFT))
        move_speed = 10.0f;

    // update keyboard movement: WASD first-person
    if (the_app->key_pressed(RIZZ_APP_KEYCODE_A) || the_app->key_pressed(RIZZ_APP_KEYCODE_LEFT)) {
        the_camera->fps_strafe(&g_draw3d.cam, -move_speed * dt);
    }

    if (the_app->key_pressed(RIZZ_APP_KEYCODE_D) || the_app->key_pressed(RIZZ_APP_KEYCODE_RIGHT)) {
        the_camera->fps_strafe(&g_draw3d.cam, move_speed * dt);
    }

    if (the_app->key_pressed(RIZZ_APP_KEYCODE_W) || the_app->key_pressed(RIZZ_APP_KEYCODE_UP)) {
        the_camera->fps_forward(&g_draw3d.cam, move_speed * dt);
    }

    if (the_app->key_pressed(RIZZ_APP_KEYCODE_S) || the_app->key_pressed(RIZZ_APP_KEYCODE_DOWN)) {
        the_camera->fps_forward(&g_draw3d.cam, -move_speed * dt);
    }

    for (int i = 0; i < 4; i++) {
        agent_state* agent = &g_draw3d.agents[i];
        if (agent->walking && agent->path.array != NULL) {
            sx_vec3 target = sx_vec3v2(agent->path.array[agent->cindex], 0);
            sx_vec3 dir = sx_vec3_sub(target, agent->transform.pos);
            float dist = sx_vec3_len(dir);
            sx_vec3 move = dist > 0.01f ? sx_vec3_norm(dir) : dir;
            agent->transform.pos = sx_vec3_add(agent->transform.pos, sx_vec3_mulf(move, dt * 2));
            agent->transform.rot = sx_mat3_rotate(sx_atan2(dir.y, dir.x) + SX_PI * -0.5f);
            if (dist < 0.02f) {
                agent->cindex++;
                if (agent->cindex >= sx_array_count(agent->path.array)) {
                    agent->cindex = 0;
                    agent->walking = 0;
                }
            }
        } else {
            agent->waiting -= dt;
            if (agent->waiting <= 0) {
                sx_vec2 start = sx_vec2f(agent->transform.pos.x, agent->transform.pos.y);
                sx_vec2 end = random_pos();
                the_astar->findpath(&g_draw3d.astar_world, &agent->astar_agent, start, end,
                                    &agent->path);
                agent->waiting = 1;
                agent->walking = true;
            }
        }
    }


    show_debugmenu(the_imgui, the_core);

    if (the_imgui->Begin("pathfind", NULL, 0)) {
        if (the_imgui->Checkbox("Use Grid With Costs", &g_draw3d.use_grid2)) {
            g_draw3d.astar_world.cells = g_draw3d.use_grid2 ? k_celldata2 : k_celldata;
        }
        if (g_draw3d.use_grid2)
        {
            the_imgui->TextWrapped(
                "agents prefer to walk on cells with same color and avoid other colors.");
        }
    }
    the_imgui->End();
}

static void render(void)
{
    sg_pass_action pass_action = { .colors[0] = { SG_ACTION_CLEAR, { 0.25f, 0.5f, 0.75f, 1.0f } },
                                   .depth = { SG_ACTION_CLEAR, 1.0f } };
    the_gfx->staged.begin(g_draw3d.stage);
    the_gfx->staged.begin_default_pass(&pass_action, the_app->width(), the_app->height());

    sx_mat4 proj, view;
    the_camera->perspective_mat(&g_draw3d.cam.cam, &proj);
    the_camera->view_mat(&g_draw3d.cam.cam, &view);
    sx_mat4 viewproj = sx_mat4_mul(&proj, &view);

    {    // draw cells
        static sx_box boxes[GRID_WIDTH * GRID_HEIGHT];
        static sx_color colors[GRID_WIDTH * GRID_HEIGHT];
        const sx_vec3 vec3half = (sx_vec3){ .x = 0.5f, .y = 0.5f, .z = 0.05f };

        for (int i = 0; i < GRID_WIDTH * GRID_HEIGHT; i++) {
            int x = (int)(i % GRID_WIDTH);
            int y = (int)(i / GRID_WIDTH);
            boxes[i] = sx_box_set(sx_tx3d_setf((float)x, (float)y, 0, 0, 0, 0), vec3half);
            colors[i] = k_celltype_colors[g_draw3d.astar_world.cells[i]];
        }

        the_3d->debug.draw_boxes(boxes, GRID_WIDTH * GRID_HEIGHT, &viewproj, RIZZ_3D_DEBUG_MAPTYPE_CHECKER, colors);
    }

    // draw path lines
    for (int i = 0; i < 4; i++) {
        agent_state* agent = &g_draw3d.agents[i];
        if (agent->path.array != NULL) {
            sx_vec3 line[16];
            int count = sx_array_count(agent->path.array);
            for (int i = 0; i < count; i++)
                line[i] = sx_vec3v2(agent->path.array[i], 0.5f);

            the_3d->debug.draw_path(line, count, &viewproj, agent->color);
        }
    }

    // model
    const rizz_model* model = the_3d->model.get(g_draw3d.agent_model);
    draw3d_vertex_shader_uniforms vs_uniforms = { .viewproj_mat = viewproj };
    draw3d_fragment_shader_uniforms fs_uniforms = { .light_dir = sx_vec3_norm(g_draw3d.light_dir) };
    the_gfx->staged.apply_pipeline(g_draw3d.pip);

    const rizz_model_mesh* mesh = &model->meshes[0];

    for (int i = 0; i < 4; i++) {
        agent_state* agent = &g_draw3d.agents[i];
        sx_mat4 node_mat = sx_tx3d_mat4(&agent->transform);
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
            fs_uniforms.color =
                sx_vec3f(agent->color.r / 255.0f, agent->color.g / 255.0f, agent->color.b / 255.0f);
            the_gfx->staged.apply_uniforms(SG_SHADERSTAGE_FS, 0, &fs_uniforms, sizeof(fs_uniforms));
            the_gfx->staged.apply_bindings(&bind);
            the_gfx->staged.draw(0, submesh->num_indices, 1);
        }
    }

    the_gfx->staged.end_pass();
    the_gfx->staged.end();
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
        the_3d = (rizz_api_3d*)plugin->api->get_api_byname("3dtools", 0);
        the_astar = (rizz_api_astar*)plugin->api->get_api_byname("astar", 0);

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

rizz_plugin_decl_event_handler(pathfind, e)
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
    conf->app_name = "";
    conf->app_version = 1000;
    conf->app_title = "pathfind";
    conf->app_flags |= RIZZ_APP_FLAG_HIGHDPI;
    conf->log_level = RIZZ_LOG_LEVEL_DEBUG;
    conf->window_width = EXAMPLES_DEFAULT_WIDTH;
    conf->window_height = EXAMPLES_DEFAULT_HEIGHT;
    conf->multisample_count = 4;
    conf->swap_interval = 1;
    conf->plugins[0] = "imgui";
    conf->plugins[1] = "3dtools";
    conf->plugins[2] = "astar";
}