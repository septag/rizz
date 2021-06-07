#include "sx/os.h"
#include "sx/string.h"
#include "sx/rng.h"
#include "sx/math-vec.h"

#include "rizz/imgui-extra.h"
#include "rizz/imgui.h"
#include "rizz/rizz.h"

#include "../common.h"

RIZZ_STATE static rizz_api_core* the_core;
RIZZ_STATE static rizz_api_gfx* the_gfx;
RIZZ_STATE static rizz_api_app* the_app;
RIZZ_STATE static rizz_api_imgui* the_imgui;
RIZZ_STATE static rizz_api_asset* the_asset;
RIZZ_STATE static rizz_api_camera* the_camera;
RIZZ_STATE static rizz_api_vfs* the_vfs;

#define MAX_PARTICLES 10000
#define SPREAD 400.0f

typedef struct {
    int _p1[4];
    float _p2[4];
} nbody_compute_params;

typedef struct {
    sx_vec3 pos;
    sx_vec2 uv;
    sx_color color;
} nbody_draw_vertex;

static rizz_vertex_layout k_vertex_layout = {
    .attrs[0] = { .semantic = "POSITION", .offset = offsetof(nbody_draw_vertex, pos) },
    .attrs[1] = { .semantic = "TEXCOORD", .offset = offsetof(nbody_draw_vertex, uv) },
    .attrs[2] = { .semantic = "COLOR",
                  .offset = offsetof(nbody_draw_vertex, color),
                  .format = SG_VERTEXFORMAT_UBYTE4N }
};

typedef struct {
    sx_vec4 pos;
    sx_vec4 vel;
} nbody_particle;

typedef struct {
    sx_mat4 inv_view;
    sx_mat4 vp;
} nbody_vs_params;

typedef struct {
    sx_rng rng;
    rizz_gfx_stage stage;
    rizz_asset img;
    rizz_asset draw_shader;
    rizz_asset particle_shader;
    sg_buffer vbuff;
    sg_buffer ibuff;
    sg_buffer particle_buff1;
    sg_buffer particle_buff2;
    sg_pipeline draw_pip;
    sg_pipeline particle_pip;
    float camera_orbit;
    float simulation_speed;
    float damping;
    rizz_camera_fps cam;
} nbody_state;

RIZZ_STATE static nbody_state g_nbody;

static void orbit_camera()
{
    float x = SPREAD * 3.0f * sx_cos(g_nbody.camera_orbit);
    float y = SPREAD * 3.0f * sx_sin(g_nbody.camera_orbit);
    sx_vec3 pos = sx_vec3f(x, y, 3 * SPREAD);
    the_camera->fps_lookat(&g_nbody.cam, pos, SX_VEC3_ZERO, SX_VEC3_UNITZ);
}

static void load_particles(nbody_particle* particles, sx_vec3 center, sx_vec4 velocity,
                           float spread, int num_particles)
{
    for (int i = 0; i < num_particles; i++) {
        // generate points within 'spread' sphere
        sx_vec3 delta = sx_vec3f(spread, spread, spread);
        while (sx_vec3_dot(delta, delta) > spread * spread) {
            delta = sx_vec3f((sx_rng_genf(&g_nbody.rng) * 2.0f - 1.0f) * spread,
                             (sx_rng_genf(&g_nbody.rng) * 2.0f - 1.0f) * spread,
                             (sx_rng_genf(&g_nbody.rng) * 2.0f - 1.0f) * spread);
        }

        particles[i].pos = sx_vec4v3(sx_vec3_add(center, delta), 10000.0f * 10000.0f);
        particles[i].vel = velocity;
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

    sx_rng_seed_time(&g_nbody.rng);

    // register main graphics stage.
    // at least one stage should be registered if you want to draw anything
    g_nbody.stage = the_gfx->stage_register("main", (rizz_gfx_stage){ .id = 0 });
    sx_assert(g_nbody.stage.id);

    // quad vertices:
    // clang-format off
    static nbody_draw_vertex vertices[] = { 
        {{{-1.0f, 0.0f, -1.0f}}, {{0.0f, 1.0f}},  {{255, 255, 255, 255}}},
        {{{1.0f, 0.0f, -1.0f}}, {{1.0f, 1.0f}}, {{255, 255, 255, 255}}},
        {{{1.0f, 0.0f,  1.0f}}, {{1.0f, 0.0f}}, {{255, 255, 255, 255}}}, 
        {{{-1.0f, 0.0f,  1.0f}}, {{0.0f, 0.0f}}, {{255, 255, 255, 255}}}
    };
    // clang-format on

    uint16_t indices[] = { 0, 2, 1, 2, 0, 3 };

    const sx_alloc* tmp_alloc = the_core->tmp_alloc_push();
    sx_scope(the_core->tmp_alloc_pop()) {
        // buffers
        g_nbody.vbuff = the_gfx->make_buffer(&(sg_buffer_desc){ .usage = SG_USAGE_IMMUTABLE,
                                                                .type = SG_BUFFERTYPE_VERTEXBUFFER,
                                                                .size = sizeof(nbody_draw_vertex) * 4,
                                                                .content = vertices });

        g_nbody.ibuff = the_gfx->make_buffer(&(sg_buffer_desc){ .usage = SG_USAGE_IMMUTABLE,
                                                                .type = SG_BUFFERTYPE_INDEXBUFFER,
                                                                .size = sizeof(uint16_t) * 6,
                                                                .content = indices });

        // shader
        // this shader is built with `glslcc` that reside in `/tools` directory.
        // see `glslcc --help` for details
        char shader_path[RIZZ_MAX_PATH];
        g_nbody.draw_shader = the_asset->load(
            "shader", ex_shader_path(shader_path, sizeof(shader_path), "/assets/shaders", "nbody.sgs"),
            NULL, 0, NULL, 0);
        g_nbody.particle_shader = the_asset->load(
            "shader",
            ex_shader_path(shader_path, sizeof(shader_path), "/assets/shaders", "nbody.comp.sgs"), NULL,
            0, NULL, 0);

        // pipelines
        g_nbody.draw_pip = the_gfx->make_pipeline(the_gfx->shader_bindto_pipeline(
            the_gfx->shader_get(g_nbody.draw_shader),
            &(sg_pipeline_desc){ .layout.buffers[0].stride = sizeof(nbody_draw_vertex),
                                 .shader = the_gfx->shader_get(g_nbody.draw_shader)->shd,
                                 .index_type = SG_INDEXTYPE_UINT16,
                                 .rasterizer = { .cull_mode = SG_CULLMODE_BACK, .sample_count = 1 },
                                 .blend = { .enabled = true,
                                            .src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA,
                                            .dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA } },
            &k_vertex_layout));


        nbody_particle* particles =
            (nbody_particle*)sx_malloc(tmp_alloc, sizeof(nbody_particle) * MAX_PARTICLES);
        sx_assert(particles);
        float center_spread = SPREAD * 0.5f;
        load_particles(particles, sx_vec3f(center_spread, 0, 0),
                       sx_vec4f(0, 0, -20.0f, 1.0f / 10000.0f / 10000.0f), SPREAD, MAX_PARTICLES / 2);
        load_particles(particles + MAX_PARTICLES / 2, sx_vec3f(-center_spread, 0, 0),
                       sx_vec4f(0, 0, 20.0f, 1.0f / 10000.0f / 10000.0f), SPREAD, MAX_PARTICLES / 2);

        g_nbody.particle_pip = the_gfx->make_pipeline(&(sg_pipeline_desc){
            .shader = the_gfx->shader_get(g_nbody.particle_shader)->shd });

        // particle buffers
        g_nbody.particle_buff1 =
            the_gfx->make_buffer(&(sg_buffer_desc){ .size = sizeof(nbody_particle) * MAX_PARTICLES,
                                                    .type = SG_BUFFERTYPE_RAW,
                                                    .shader_write = true,
                                                    .content = particles });

        g_nbody.particle_buff2 =
            the_gfx->make_buffer(&(sg_buffer_desc){ .size = sizeof(nbody_particle) * MAX_PARTICLES,
                                                    .type = SG_BUFFERTYPE_RAW,
                                                    .shader_write = true });

        // particle
        g_nbody.img = the_asset->load("texture", "/assets/textures/particle.dds",
                                      &(rizz_texture_load_params){ 0 }, 0, NULL, 0);

        //////////////////////////////////////////////////////////////////////////////////////////
        // camera
        // projection: setup for perspective, SPREAD*3.0f away from the center
        // view: Z-UP Y-Forward (like blender)
        sx_vec2 screen_size;
        the_app->window_size(&screen_size);
        const float view_width = screen_size.x;
        const float view_height = screen_size.y;
        the_camera->fps_init(&g_nbody.cam, 50.0f, sx_rectwh(0, 0, view_width, view_height), 10.0f,
                             500000.0f);
        the_camera->fps_lookat(&g_nbody.cam, sx_vec3f(0, -SPREAD * 3.0f, SPREAD * 3.0f), SX_VEC3_ZERO,
                               SX_VEC3_UNITZ);

        g_nbody.camera_orbit = -SX_PIHALF;

        g_nbody.simulation_speed = 0.1f;
        g_nbody.damping = 1.0f;
    } // scope

    return true;
}

static void shutdown()
{
    if (g_nbody.vbuff.id)
        the_gfx->destroy_buffer(g_nbody.vbuff);
    if (g_nbody.ibuff.id)
        the_gfx->destroy_buffer(g_nbody.ibuff);
    if (g_nbody.img.id)
        the_asset->unload(g_nbody.img);
    if (g_nbody.draw_shader.id)
        the_asset->unload(g_nbody.draw_shader);
    if (g_nbody.draw_pip.id)
        the_gfx->destroy_pipeline(g_nbody.draw_pip);
    if (g_nbody.particle_shader.id)
        the_asset->unload(g_nbody.particle_shader);
    if (g_nbody.particle_pip.id)
        the_gfx->destroy_pipeline(g_nbody.particle_pip);
    if (g_nbody.particle_buff1.id)
        the_gfx->destroy_buffer(g_nbody.particle_buff1);
    if (g_nbody.particle_buff2.id)
        the_gfx->destroy_buffer(g_nbody.particle_buff2);
}

static void update(float dt) {}

static void render()
{
    sg_pass_action pass_action = { .colors[0] = { SG_ACTION_CLEAR, { 0.0f, 0.0f, 0.0f, 1.0f } },
                                   .depth = { SG_ACTION_CLEAR, 1.0f } };
    the_gfx->staged.begin(g_nbody.stage);

    // dispatch CS to calculate particle information
    {
        int dimx = (int)sx_ceil(MAX_PARTICLES / 128.0f);

        the_gfx->staged.apply_pipeline(g_nbody.particle_pip);
        the_gfx->staged.apply_bindings(&(sg_bindings){
            .cs_buffers[0] = g_nbody.particle_buff1, .cs_buffer_uavs[0] = g_nbody.particle_buff2 });

        the_gfx->staged.apply_uniforms(
            SG_SHADERSTAGE_CS, 0,
            &(nbody_compute_params){ { MAX_PARTICLES, dimx, 0, 0 },
                                     { g_nbody.simulation_speed, g_nbody.damping, 0, 0 } },
            sizeof(nbody_compute_params));

        the_gfx->staged.dispatch(dimx, 1, 1);

        sx_swap(g_nbody.particle_buff1, g_nbody.particle_buff2, sg_buffer);    // ping-pong buffers
    }

    the_gfx->staged.begin_default_pass(&pass_action, the_app->width(), the_app->height());

    // draw textured quad with the CS processed texture
    {
        sx_mat4 proj, view;
        the_camera->perspective_mat(&g_nbody.cam.cam, &proj);
        the_camera->view_mat(&g_nbody.cam.cam, &view);

        the_gfx->staged.apply_pipeline(g_nbody.draw_pip);
        the_gfx->staged.apply_bindings(
            &(sg_bindings){ .vertex_buffers[0] = g_nbody.vbuff,
                            .index_buffer = g_nbody.ibuff,
                            .fs_images[0] = the_gfx->texture_get(g_nbody.img)->img,
                            .vs_buffers[0] = g_nbody.particle_buff1 });

        rizz_camera* cam = &g_nbody.cam.cam;
        sx_mat4 inv_view = sx_mat4v(sx_vec4f(cam->right.x, cam->right.y, cam->right.z, 0),
                                    sx_vec4f(-cam->forward.x, -cam->forward.y, -cam->forward.z, 0),
                                    sx_vec4f(cam->up.x, cam->up.y, cam->up.z, 0), SX_VEC4_ZERO);

        the_gfx->staged.apply_uniforms(
            SG_SHADERSTAGE_VS, 0,
            &(nbody_vs_params){ .inv_view = inv_view, .vp = sx_mat4_mul(&proj, &view) },
            sizeof(nbody_vs_params));
        the_gfx->staged.draw(0, 6, MAX_PARTICLES);
    }

    the_gfx->staged.end_pass();
    the_gfx->staged.end();

    // Use imgui UI
    show_debugmenu(the_imgui, the_core);
    
    the_imgui->SetNextWindowContentSize(sx_vec2f(200.0f, 150.0f));
    if (the_imgui->Begin("nbody", NULL, 0)) {
        the_imgui->LabelText("Fps", "%.3f", the_core->fps());
        the_imgui->SliderFloat("Speed", &g_nbody.simulation_speed, 0.01f, 0.5f, "%.2f", 0);
        the_imgui->SliderFloat("Damping", &g_nbody.damping, 0.9f, 1.1f, "%.3f", 0);
    }
    the_imgui->End();
}

rizz_plugin_decl_main(nbody, plugin, e)
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
    static float last_mouse_x = 0;
    float dt = (float)sx_tm_sec(the_core->delta_tick());
    const float rotate_speed = 5.0f;

    switch (e->type) {
    case RIZZ_APP_EVENTTYPE_SUSPENDED:
        break;
    case RIZZ_APP_EVENTTYPE_RESTORED:
        break;
    case RIZZ_APP_EVENTTYPE_MOUSE_DOWN:
        mouse_down = true;
        last_mouse_x = e->mouse_x;
        break;
    case RIZZ_APP_EVENTTYPE_MOUSE_UP:
    case RIZZ_APP_EVENTTYPE_MOUSE_LEAVE:
        mouse_down = false;
        break;
    case RIZZ_APP_EVENTTYPE_MOUSE_MOVE:
        if (mouse_down) {
            float dx = sx_torad(e->mouse_x - last_mouse_x) * rotate_speed * dt;
            last_mouse_x = e->mouse_x;
            g_nbody.camera_orbit += dx;
            orbit_camera();
        }
        break;
    default:
        break;
    }
}

rizz_game_decl_config(conf)
{
    conf->app_name = "nbody";
    conf->app_version = 1000;
    conf->app_title = "nbody";
    conf->app_flags |= RIZZ_APP_FLAG_HIGHDPI;
    conf->window_width = EXAMPLES_DEFAULT_WIDTH;
    conf->window_height = EXAMPLES_DEFAULT_HEIGHT;
    conf->swap_interval = 1;
    conf->plugins[0] = "imgui";
}
