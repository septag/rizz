#include "sx/allocator.h"
#include "sx/math.h"
#include "sx/os.h"
#include "sx/string.h"
#include "sx/timer.h"

#include "rizz/app.h"
#include "rizz/asset.h"
#include "rizz/camera.h"
#include "rizz/core.h"
#include "rizz/entry.h"
#include "rizz/graphics.h"
#include "rizz/imgui-extra.h"
#include "rizz/imgui.h"
#include "rizz/plugin.h"
#include "rizz/vfs.h"

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

    //
    rizz_asset csshader_asset;
    sg_shader csshader;

    // sg_image csout;
    sg_buffer csin1;
    sg_buffer csin2;
    sg_buffer csout;

    sg_bindings csbindings;
    sg_pipeline cspip;
} cs_state;

RIZZ_STATE static cs_state g_cs;

#define NUM_ELEMENTS 1024

typedef struct cs_type_t {
    int i;
    float f;
} cs_type;

static bool init()
{
#if SX_PLATFORM_ANDROID
    the_vfs->mount_android_assets("/assets");
#else
    // mount `/asset` directory
    char asset_dir[RIZZ_MAX_PATH];
    sx_os_path_join(asset_dir, sizeof(asset_dir), EXAMPLES_ROOT, "assets");    // "/examples/assets"
    the_vfs->mount(asset_dir, "/assets");
    the_vfs->watch_mounts();
#endif


    // load assets metadata cache to speedup asset loading
    // always do this after you have mounted all virtual directories
    the_asset->load_meta_cache();

    // register main graphics stage.
    // at least one stage should be registered if you want to draw anything
    g_cs.stage = the_gfx->stage_register("main", (rizz_gfx_stage){ .id = 0 });
    sx_assert(g_cs.stage.id);

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
    g_cs.vbuff = the_gfx->make_buffer(&(sg_buffer_desc){ .usage = SG_USAGE_IMMUTABLE,
                                                         .type = SG_BUFFERTYPE_VERTEXBUFFER,
                                                         .size = sizeof(vertices),
                                                         .content = vertices });

    g_cs.ibuff = the_gfx->make_buffer(&(sg_buffer_desc){ .usage = SG_USAGE_IMMUTABLE,
                                                         .type = SG_BUFFERTYPE_INDEXBUFFER,
                                                         .size = sizeof(indices),
                                                         .content = indices });

    // shader
    // this shader is built with `glslcc` that reside in `/tools` directory.
    // see `glslcc --help` for details
    char shader_path[RIZZ_MAX_PATH];
    g_cs.shader = the_asset->load(
        "shader", ex_shader_path(shader_path, sizeof(shader_path), "/assets/shaders", "quad.sgs"),
        NULL, 0, NULL, 0);

    // pipeline
    sg_pipeline_desc pip_desc = {
        .layout.buffers[0].stride = 20,    // sizeof each vertex (float[3] + float[2])
        .shader = ((rizz_shader*)the_asset->obj(g_cs.shader).ptr)->shd,
        .index_type = SG_INDEXTYPE_UINT16,
        .rasterizer = { .cull_mode = SG_CULLMODE_BACK, .sample_count = 4 }
    };
    g_cs.pip = the_gfx->make_pipeline(the_gfx->shader_bindto_pipeline(
        the_asset->obj(g_cs.shader).ptr, &pip_desc, &k_vertex_layout));

    // bindings
    g_cs.bindings = (sg_bindings){ .vertex_buffers[0] = g_cs.vbuff, .index_buffer = g_cs.ibuff };

    g_cs.img = the_asset->load("texture", "/assets/textures/texfmt_rgba8.png",
                               &(rizz_texture_load_params){ 0 }, 0, NULL, 0);

    // compute-shader stuff
    sg_image_desc csout_desc = { .type = SG_IMAGETYPE_2D,
                                 .shader_write = true,
                                 .pixel_format = SG_PIXELFORMAT_RGBA8,
                                 .width = 512,
                                 .height = 512 };
    // g_quad.csout = the_gfx->make_image(&csout_desc);
    const sx_alloc* tmp_alloc = the_core->tmp_alloc_push();
    cs_type* in1 = (cs_type*)sx_malloc(tmp_alloc, sizeof(cs_type) * NUM_ELEMENTS);
    cs_type* in2 = (cs_type*)sx_malloc(tmp_alloc, sizeof(cs_type) * NUM_ELEMENTS);
    for (int i = 0; i < NUM_ELEMENTS; i++) {
        in1[i].f = (float)i;
        in1[i].i = i;
        in2[i].f = (float)i;
        in2[i].i = i;
    }

    g_cs.csin1 = the_gfx->make_buffer(&(sg_buffer_desc){ .type = SG_BUFFERTYPE_RAW,
                                                         .size = NUM_ELEMENTS * sizeof(cs_type),
                                                         .stride = sizeof(cs_type),
                                                         .content = in1 });
    g_cs.csin2 = the_gfx->make_buffer(&(sg_buffer_desc){ .type = SG_BUFFERTYPE_RAW,
                                                         .size = NUM_ELEMENTS * sizeof(cs_type),
                                                         .stride = sizeof(cs_type),
                                                         .content = in2 });
    the_core->tmp_alloc_pop();

    g_cs.csout = the_gfx->make_buffer(&(sg_buffer_desc){ .type = SG_BUFFERTYPE_RAW,
                                                         .size = NUM_ELEMENTS * sizeof(cs_type),
                                                         .stride = sizeof(cs_type),
                                                         .shader_write = true });

    g_cs.csshader_asset = the_asset->load(
        "shader",
        ex_shader_path(shader_path, sizeof(shader_path), "/assets/shaders", "cs.comp.sgs"), NULL, 0,
        NULL, 0);

    sg_pipeline_desc cspip_desc = {
        .shader = ((rizz_shader*)the_asset->obj(g_cs.csshader_asset).ptr)->shd,
    };
    g_cs.cspip = the_gfx->make_pipeline(&cspip_desc);

    g_cs.csbindings = (sg_bindings){ 0 };

    //////////////////////////////////////////////////////////////////////////////////////////
    // camera
    // projection: setup for ortho, total-width = 10 units
    // view: Z-UP Y-Forward (like blender)
    sx_vec2 screen_size = the_app->sizef();
    const float view_width = 5.0f;
    const float view_height = screen_size.y * view_width / screen_size.x;
    the_camera->fps_init(&g_cs.cam, 50.0f,
                         sx_rectwh(-view_width, -view_height, view_width, view_height), 0.1f,
                         100.0f);
    the_camera->fps_lookat(&g_cs.cam, sx_vec3f(0, -4.0f, 0.0), SX_VEC3_ZERO, SX_VEC3_UNITZ);

    return true;
}

static void shutdown()
{
    if (g_cs.vbuff.id)
        the_gfx->destroy_buffer(g_cs.vbuff);
    if (g_cs.ibuff.id)
        the_gfx->destroy_buffer(g_cs.ibuff);
    if (g_cs.img.id)
        the_asset->unload(g_cs.img);
    if (g_cs.shader.id)
        the_asset->unload(g_cs.shader);
    if (g_cs.pip.id)
        the_gfx->destroy_pipeline(g_cs.pip);
}

static void update(float dt) {}

static void render()
{
    sg_pass_action pass_action = { .colors[0] = { SG_ACTION_CLEAR, { 0.25f, 0.5f, 0.75f, 1.0f } },
                                   .depth = { SG_ACTION_CLEAR, 1.0f } };
    the_gfx->imm.begin(g_cs.stage);

    // dispatch CS to generate the texture
    {
        // g_quad.csbindings.cs_images[0] = ((rizz_texture*)the_asset->obj(g_quad.img).ptr)->img;
        // g_quad.csbindings.cs_images[1] = g_quad.csout;
        g_cs.csbindings.cs_buffers[0] = g_cs.csin1;
        g_cs.csbindings.cs_buffers[1] = g_cs.csin2;
        g_cs.csbindings.cs_buffers[2] = g_cs.csout;

        the_gfx->imm.apply_pipeline(g_cs.cspip);
        the_gfx->imm.apply_bindings(&g_cs.csbindings);
        // the_gfx->imm.dispatch(512 / 16, 512 / 16, 1);
        the_gfx->imm.dispatch(NUM_ELEMENTS, 1, 1);
    }

    the_gfx->imm.begin_default_pass(&pass_action, the_app->width(), the_app->height());

    // draw textured quad with the CS processed texture
    if (0) {
        sx_mat4 proj = the_camera->ortho_mat(&g_cs.cam.cam);
        sx_mat4 view = the_camera->view_mat(&g_cs.cam.cam);

        quad_matrices mats = { .mvp = sx_mat4_mul(&proj, &view) };

        // g_quad.bindings.fs_images[0] = g_quad.csout;
        the_gfx->imm.apply_pipeline(g_cs.pip);
        the_gfx->imm.apply_bindings(&g_cs.bindings);
        the_gfx->imm.apply_uniforms(SG_SHADERSTAGE_VS, 0, &mats, sizeof(mats));
        the_gfx->imm.draw(0, 6, 1);
    }

    the_gfx->imm.end_pass();
    the_gfx->imm.end();

    // Use imgui UI
    the_imgui->SetNextWindowContentSize(sx_vec2f(100.0f, 50.0f));
    if (the_imgui->Begin("cs", NULL, 0)) {
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
    conf->app_name = "cs";
    conf->app_version = 1000;
    conf->app_title = "cs";
    conf->window_width = 800;
    conf->window_height = 600;
    conf->core_flags |= RIZZ_CORE_FLAG_VERBOSE;
    conf->multisample_count = 4;
    conf->swap_interval = 2;
    conf->plugins[0] = "imgui";
}
