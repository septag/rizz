#include "sx/os.h"
#include "sx/string.h"
#include "sx/timer.h"
#include "sx/math-vec.h"

#include "rizz/rizz.h"
#include "rizz/imgui-extra.h"
#include "rizz/imgui.h"

#include "../common.h"

RIZZ_STATE static rizz_api_core* the_core;
RIZZ_STATE static rizz_api_gfx* the_gfx;
RIZZ_STATE static rizz_api_app* the_app;
RIZZ_STATE static rizz_api_imgui* the_imgui;
RIZZ_STATE static rizz_api_asset* the_asset;
RIZZ_STATE static rizz_api_camera* the_camera;
RIZZ_STATE static rizz_api_vfs* the_vfs;

typedef struct {
    sx_mat4 mvp;
} quad_matrices;

typedef struct {
    rizz_gfx_stage stage;
    sg_bindings bindings;
    sg_pipeline pip;
    rizz_asset img;
    rizz_asset shader;
    sg_buffer vbuff;
    sg_buffer ibuff;
    rizz_camera_fps cam;
} quad_state;

RIZZ_STATE static quad_state g_quad;

static bool init()
{
#if SX_PLATFORM_ANDROID || SX_PLATFORM_IOS
    the_vfs->mount_mobile_assets("/assets");
#else
    // mount `/asset` directory
    char asset_dir[RIZZ_MAX_PATH];
    sx_os_path_join(asset_dir, sizeof(asset_dir), EXAMPLES_ROOT, "assets");    // "/examples/assets"
    the_vfs->mount(asset_dir, "/assets");
#endif

    // register main graphics stage.
    // at least one stage should be registered if you want to draw anything
    g_quad.stage = the_gfx->stage_register("main", (rizz_gfx_stage){ .id = 0 });
    sx_assert(g_quad.stage.id);

    // quad vertices:
    //      pos: float[3]
    //      uv: float[2]
    // clang-format off
    static float vertices[] = { 
        -1.0f, 0.0f, -1.0f,      0.0f, 1.0f, 
         1.0f, 0.0f, -1.0f,      1.0f, 1.0f,
         1.0f, 0.0f,  1.0f,      1.0f, 0.0f, 
        -1.0f, 0.0f,  1.0f,      0.0f, 0.0f };
    // clang-format on

    static rizz_vertex_layout k_vertex_layout = {
        .attrs[0] = { .semantic = "POSITION" },
        .attrs[1] = { .semantic = "TEXCOORD", .offset = 12 },
    };


    uint16_t indices[] = { 0, 2, 1, 2, 0, 3 };

    // buffers
    g_quad.vbuff = the_gfx->make_buffer(&(sg_buffer_desc){ .usage = SG_USAGE_IMMUTABLE,
                                                           .type = SG_BUFFERTYPE_VERTEXBUFFER,
                                                           .size = sizeof(vertices),
                                                           .content = vertices });

    g_quad.ibuff = the_gfx->make_buffer(&(sg_buffer_desc){ .usage = SG_USAGE_IMMUTABLE,
                                                           .type = SG_BUFFERTYPE_INDEXBUFFER,
                                                           .size = sizeof(indices),
                                                           .content = indices });

    // shader
    // this shader is built with `glslcc` that reside in `/tools` directory.
    // see `glslcc --help` for details
    char shader_path[RIZZ_MAX_PATH];
    g_quad.shader = the_asset->load(
        "shader", ex_shader_path(shader_path, sizeof(shader_path), "/assets/shaders", "quad.sgs"),
        NULL, 0, NULL, 0);

    // pipeline
    sg_pipeline_desc pip_desc = { .layout.buffers[0].stride = 20,    // vertex size (float[3] + float[2])
                                  .shader = the_gfx->shader_get(g_quad.shader)->shd,
                                  .index_type = SG_INDEXTYPE_UINT16,
                                  .rasterizer = { .cull_mode = SG_CULLMODE_BACK } };
    g_quad.pip = the_gfx->make_pipeline(the_gfx->shader_bindto_pipeline(
        the_gfx->shader_get(g_quad.shader), &pip_desc, &k_vertex_layout));

    // bindings
    g_quad.bindings =
        (sg_bindings){ .vertex_buffers[0] = g_quad.vbuff, .index_buffer = g_quad.ibuff };

    // note: we should define .fmt property when loading .basis textures
    bool etc_fmt = the_gfx->GLES_family() || the_gfx->backend() == RIZZ_GFX_BACKEND_METAL_IOS;
    g_quad.img =
        the_asset->load("texture", "/assets/textures/logo.basis",
                        &(rizz_texture_load_params){ .fmt = etc_fmt ? SG_PIXELFORMAT_ETC2_RGB8
                                                                    : SG_PIXELFORMAT_BC1_RGBA },
                        0, NULL, 0);
    
    // camera
    // projection: setup for ortho, total-width = 10 units
    // view: Z-UP Y-Forward (like blender)
    sx_vec2 screen_size;
    the_app->window_size(&screen_size);
    const float view_width = 5.0f;
    const float view_height = screen_size.y * view_width / screen_size.x;
    the_camera->fps_init(&g_quad.cam, 50.0f,
                         sx_rectwh(-view_width, -view_height, view_width, view_height), 0.1f,
                         100.0f);
    the_camera->fps_lookat(&g_quad.cam, sx_vec3f(0, -4.0f, 0.0), SX_VEC3_ZERO, SX_VEC3_UNITZ);

    return true;
}

static void shutdown()
{
    if (g_quad.vbuff.id)
        the_gfx->destroy_buffer(g_quad.vbuff);
    if (g_quad.ibuff.id)
        the_gfx->destroy_buffer(g_quad.ibuff);
    if (g_quad.img.id)
        the_asset->unload(g_quad.img);
    if (g_quad.shader.id)
        the_asset->unload(g_quad.shader);
    if (g_quad.pip.id)
        the_gfx->destroy_pipeline(g_quad.pip);
}

static void update(float dt) {}

static void render()
{
    sg_pass_action pass_action = { .colors[0] = { SG_ACTION_CLEAR, { 0.25f, 0.5f, 0.75f, 1.0f } },
                                   .depth = { SG_ACTION_CLEAR, 1.0f } };

    the_gfx->staged.begin(g_quad.stage);
    the_gfx->staged.begin_default_pass(&pass_action, the_app->width(), the_app->height());

    // draw textured quad
    {
        sx_mat4 proj, view;
        the_camera->ortho_mat(&g_quad.cam.cam, &proj);
        the_camera->view_mat(&g_quad.cam.cam, &view);

        quad_matrices mats = { .mvp = sx_mat4_mul(&proj, &view) };

        g_quad.bindings.fs_images[0] = the_gfx->texture_get(g_quad.img)->img;
        the_gfx->staged.apply_pipeline(g_quad.pip);
        the_gfx->staged.apply_bindings(&g_quad.bindings);
        the_gfx->staged.apply_uniforms(SG_SHADERSTAGE_VS, 0, &mats, sizeof(mats));
        the_gfx->staged.draw(0, 6, 1);
    }

    the_gfx->staged.end_pass();
    the_gfx->staged.end();

    // Use imgui UI
    show_debugmenu(the_imgui, the_core);
    
    the_imgui->SetNextWindowContentSize(sx_vec2f(100.0f, 50.0f));
    if (the_imgui->Begin("quad", NULL, 0)) {
        the_imgui->LabelText("Fps", "%.3f", the_core->fps());
    }
    the_imgui->End();
}

rizz_plugin_decl_main(quad, plugin, e)
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

rizz_plugin_decl_event_handler(quad, e)
{
    switch (e->type) {
    case RIZZ_APP_EVENTTYPE_SUSPENDED:
        break;
    case RIZZ_APP_EVENTTYPE_RESTORED:
        break;
    case RIZZ_APP_EVENTTYPE_MOUSE_DOWN:
        break;
    case RIZZ_APP_EVENTTYPE_MOUSE_UP:
        break;
    case RIZZ_APP_EVENTTYPE_MOUSE_MOVE:
        break;
    default:
        break;
    }
}

rizz_game_decl_config(conf)
{
    conf->app_name = "quad";
    conf->app_version = 1000;
    conf->app_title = "02 - Quad";
    conf->app_flags |= RIZZ_APP_FLAG_HIGHDPI;
    conf->window_width = EXAMPLES_DEFAULT_WIDTH;
    conf->window_height = EXAMPLES_DEFAULT_HEIGHT;
    conf->swap_interval = 2;
    conf->plugins[0] = "imgui";
}
