#include "sx/string.h"
#include "sx/timer.h"
#include "sx/rng.h"
#include "sx/math-vec.h"
#include "sx/math-easing.h"

#include "rizz/imgui-extra.h"
#include "rizz/imgui.h"
#include "rizz/rizz.h"

#include "../common.h"

RIZZ_STATE static rizz_api_core* the_core;
RIZZ_STATE static rizz_api_gfx* the_gfx;
RIZZ_STATE static rizz_api_app* the_app;
RIZZ_STATE static rizz_api_imgui* the_imgui;
RIZZ_STATE static rizz_api_imgui_extra* the_imguix;
RIZZ_STATE static rizz_api_asset* the_asset;
RIZZ_STATE static rizz_api_imgui_extra* the_imguix;
RIZZ_STATE static rizz_api_camera* the_camera;
RIZZ_STATE static rizz_api_vfs* the_vfs;

typedef struct {
    sx_vec2 pos;
} sdf_vertex;

static rizz_vertex_layout k_sdf_vertex_layout = {
    .attrs[0] = { .semantic = "POSITION", .offset = offsetof(sdf_vertex, pos) }
};

// vertex-shader uniforms
typedef struct sdf_vs_vars {
    sx_vec4 camera_corners[4];
    sx_vec4 camera_pos;    // w = aspect ratio (x/y)
    sx_vec4 screen;
} sdf_vs_vars;

// fragment-shader uniforms
typedef struct sdf_fs_vars {
    sx_vec4 camera_pos;
    sx_vec4 sphere1;
    sx_vec4 box1;
    sx_vec4 light;    // w = shadow penumbra
    sx_vec4 anim;
    sx_mat4 shape_mat;
    sx_vec4 back_light_dir;
} sdf_fs_vars;

typedef struct {
    sx_rng rng;
    rizz_gfx_stage stage;
    rizz_camera_fps cam;
    sg_buffer vbuff;
    sg_buffer ibuff;
    rizz_asset shader;
    sg_pipeline pip;
    sx_vec4 sphere1;
    sx_vec4 box1;    // w = corner radius
    float anim_tm;
    float shape_transition;
    float shape_f1;
    float shape_f2;
    float camera_orbit;
    float light_orbit;
    rizz_tween tween;
    sx_vec3 light_dir;
    sx_vec3 back_light_dir;
    float shadow_penumbra;
    float anim_duration;
} sdf_state;

RIZZ_STATE sdf_state g_sdf;

static void orbit_camera()
{
    float x = 4.5f * sx_cos(g_sdf.camera_orbit);
    float y = 4.5f * sx_sin(g_sdf.camera_orbit);
    sx_vec3 pos = sx_vec3f(x, y, 1.5f);
    the_camera->fps_lookat(&g_sdf.cam, pos, sx_vec3f(0, 0, 1.0f), SX_VEC3_UNITZ);
}

static void orbit_lights()
{
    float x = sx_cos(g_sdf.light_orbit);
    float y = sx_sin(g_sdf.light_orbit);
    g_sdf.light_dir = sx_vec3_norm(sx_vec3f(-x, -y, -1.0f));

    x = sx_cos(g_sdf.light_orbit + SX_PI * 0.8f);
    y = sx_sin(g_sdf.light_orbit + SX_PI * 0.8f);
    g_sdf.back_light_dir = sx_vec3_norm(sx_vec3f(-x, -y, -1.0f));
}

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

    sx_rng_seed_time(&g_sdf.rng);
    
    g_sdf.stage = the_gfx->stage_register("main", (rizz_gfx_stage){ .id = 0 });
    sx_assert(g_sdf.stage.id);

    // clang-format off
    sdf_vertex vertices[] = { 
        {{{-1.0f, -1.0f}}}, 
        {{{1.0f,  -1.0f}}},
        {{{1.0f,  1.0f}}}, 
        {{{-1.0f, 1.0f}}} 
    };
    // clang-format on

    uint16_t indices[] = { 0, 2, 1, 0, 3, 2 };
    g_sdf.vbuff = the_gfx->make_buffer(&(sg_buffer_desc){ .usage = SG_USAGE_IMMUTABLE,
                                                          .type = SG_BUFFERTYPE_VERTEXBUFFER,
                                                          .size = sizeof(vertices),
                                                          .content = vertices });
    g_sdf.ibuff = the_gfx->make_buffer(&(sg_buffer_desc){ .usage = SG_USAGE_IMMUTABLE,
                                                          .type = SG_BUFFERTYPE_INDEXBUFFER,
                                                          .size = sizeof(indices),
                                                          .content = indices });

    char shader_path[RIZZ_MAX_PATH];
    g_sdf.shader = the_asset->load(
        "shader", ex_shader_path(shader_path, sizeof(shader_path), "/assets/shaders", "sdf.sgs"),
        NULL, 0, NULL, 0);

    sg_pipeline_desc pip_desc = { .layout.buffers[0].stride = sizeof(sdf_vertex),
                                  .shader = the_gfx->shader_get(g_sdf.shader)->shd,
                                  .index_type = SG_INDEXTYPE_UINT16,
                                  .rasterizer = { .cull_mode = SG_CULLMODE_BACK } };
    g_sdf.pip = the_gfx->make_pipeline(the_gfx->shader_bindto_pipeline(
        the_asset->obj(g_sdf.shader).ptr, &pip_desc, &k_sdf_vertex_layout));

    // camera
    // view: Z-UP Y-Forward (like blender)
    the_camera->fps_init(&g_sdf.cam, 60.0f, sx_rectf(-0.5f, -0.5f, 0.5f, 0.5f), 1.0f, 100.0f);

    g_sdf.camera_orbit = g_sdf.light_orbit = -SX_PIHALF;
    orbit_camera();
    orbit_lights();

    g_sdf.sphere1.w = 0.5f;
    g_sdf.box1 = sx_vec4f(2.0, 2.0f, 0.2f, 0.1f);
    g_sdf.shape_f1 = 5.0f;
    g_sdf.shape_f2 = 6.0f;
    g_sdf.shadow_penumbra = 5.0f;
    g_sdf.anim_duration = 6.0f;

    return true;
}

static void shutdown()
{
    if (g_sdf.vbuff.id)
        the_gfx->destroy_buffer(g_sdf.vbuff);
    if (g_sdf.ibuff.id)
        the_gfx->destroy_buffer(g_sdf.ibuff);
    if (g_sdf.shader.id)
        the_asset->unload(g_sdf.shader);
    if (g_sdf.pip.id)
        the_gfx->destroy_pipeline(g_sdf.pip);
}

static void update(float dt)
{
    // update sphere position (bouncing animation)
    const float max_height = 2.0f;
    float t = rizz_tween_update(&g_sdf.tween, dt, g_sdf.anim_duration);
    float z = sx_easein_bounce(sx_linearstep(t, 0, 0.5f));
    const float mid_range_min = 0.45f;
    const float mid_range_max = 0.55f;
    if (t <= mid_range_min) {
        float k = sx_linearstep(t, 0, mid_range_min);
        z = sx_easein_bounce(k);
        g_sdf.shape_transition = sx_easein_expo(k);
    } else if (t > mid_range_min && t <= mid_range_max) {
        z = 1.0f;
    } else if (t > mid_range_max) {
        float k = 1.0f - sx_linearstep(t, mid_range_max, 1.0f);
        z = sx_easein_bounce(k);
        g_sdf.shape_transition = sx_easein_expo(k);
    }

    g_sdf.sphere1.z = z * (max_height + 1.0f) - 1.0f;
    g_sdf.anim_tm = z;

    // loop
    if (t >= 1.0f) {
        g_sdf.tween.tm = 0;
        g_sdf.shape_f1 = (float)sx_rng_gen_rangei(&g_sdf.rng, 5, 10);
        g_sdf.shape_f2 = g_sdf.shape_f1 + 1.0f + sx_rng_genf(&g_sdf.rng) * (g_sdf.shape_f1 - 1.0f);
    }

    // Use imgui UI
    show_debugmenu(the_imgui, the_core);
    
    the_imgui->SetNextWindowContentSize(sx_vec2f(100.0f, 50.0f));
    if (the_imgui->Begin("SDF", NULL, 0)) {
        the_imgui->LabelText("Fps", "%.3f", the_core->fps());
        float deg = sx_todeg(g_sdf.light_orbit);
        if (the_imgui->SliderFloat("Light", &deg, -180.0f, 180.0f, "%.1f", 0)) {
            g_sdf.light_orbit = sx_torad(deg);
            orbit_lights();
        }

        the_imgui->SliderFloat("shadow", &g_sdf.shadow_penumbra, 2.0f, 100.0f, "%.1f", 0);
        the_imgui->SliderFloat("duration", &g_sdf.anim_duration, 1.0f, 10.0f, "%.1f", 0);
    }
    the_imgui->End();
}

static void render()
{
    sg_pass_action pass_action = { .colors[0] = { SG_ACTION_DONTCARE },
                                   .depth = { SG_ACTION_DONTCARE } };
    the_gfx->staged.begin(g_sdf.stage);
    the_gfx->staged.begin_default_pass(&pass_action, the_app->width(), the_app->height());

    sx_vec3 frustum[8];
    sdf_vs_vars vs_vars;
    sdf_fs_vars fs_vars;

    sx_vec2 sizef;
    the_app->window_size(&sizef);

    the_camera->calc_frustum_points(&g_sdf.cam.cam, frustum);
    vs_vars.camera_corners[0] = sx_vec4v3(frustum[0], 0);
    vs_vars.camera_corners[1] = sx_vec4v3(frustum[1], 0);
    vs_vars.camera_corners[2] = sx_vec4v3(frustum[2], 0);
    vs_vars.camera_corners[3] = sx_vec4v3(frustum[3], 0);

    vs_vars.camera_pos = sx_vec4v3(g_sdf.cam.cam.pos, 0);

    vs_vars.screen = sx_vec4f(sizef.x < sizef.y ? (1.0f + (sizef.x / sizef.y)) : 1.0f,
                              sizef.x > sizef.y ? (sizef.x / sizef.y) : 1.0f, 0, 0);
    fs_vars.camera_pos = sx_vec4v3(g_sdf.cam.cam.pos, 1.0f);
    fs_vars.sphere1 = g_sdf.sphere1;
    fs_vars.box1 = g_sdf.box1;
    fs_vars.light =
        sx_vec4f(g_sdf.light_dir.x, g_sdf.light_dir.y, g_sdf.light_dir.z, g_sdf.shadow_penumbra);
    fs_vars.back_light_dir = sx_vec4v3(g_sdf.back_light_dir, 0);
    fs_vars.anim = sx_vec4f(g_sdf.anim_tm, g_sdf.shape_transition, g_sdf.shape_f1, g_sdf.shape_f2);
    {
        sx_mat4 rot = sx_mat4_rotateX(SX_PIHALF);
        sx_mat4 translate = sx_mat4_translate(0, 0.0f, 2.0f);
        sx_mat4 mat = sx_mat4_mul(&translate, &rot);
        fs_vars.shape_mat = sx_mat4_inv_transform(&mat);
    }

    // draw quad
    {
        sg_bindings bindings = { .vertex_buffers[0] = g_sdf.vbuff, .index_buffer = g_sdf.ibuff };
        the_gfx->staged.apply_pipeline(g_sdf.pip);
        the_gfx->staged.apply_bindings(&bindings);
        the_gfx->staged.apply_uniforms(SG_SHADERSTAGE_VS, 0, &vs_vars, sizeof(vs_vars));
        the_gfx->staged.apply_uniforms(SG_SHADERSTAGE_FS, 0, &fs_vars, sizeof(fs_vars));

        the_gfx->staged.draw(0, 6, 1);
    }

    the_gfx->staged.end_pass();
    the_gfx->staged.end();
}

rizz_plugin_decl_main(sdf, plugin, e)
{
    switch (e) {
    case RIZZ_PLUGIN_EVENT_STEP:
        update(the_core->delta_time());
        render();
        break;

    case RIZZ_PLUGIN_EVENT_INIT:
        the_core = plugin->api->get_api(RIZZ_API_CORE, 0);
        the_gfx = plugin->api->get_api(RIZZ_API_GFX, 0);
        the_app = plugin->api->get_api(RIZZ_API_APP, 0);
        the_asset = plugin->api->get_api(RIZZ_API_ASSET, 0);
        the_vfs = plugin->api->get_api(RIZZ_API_VFS, 0);
        the_camera = plugin->api->get_api(RIZZ_API_CAMERA, 0);
        the_imgui = plugin->api->get_api_byname("imgui", 0);
        the_imguix = plugin->api->get_api_byname("imgui_extra", 0);

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

rizz_plugin_decl_event_handler(sdf, e)
{
    static bool mouse_down = false;
    static float last_mouse_x = 0;
    float dt = (float)sx_tm_sec(the_core->delta_tick());
    const float rotate_speed = 5.0f;

    switch (e->type) {
    case RIZZ_APP_EVENTTYPE_MOUSE_DOWN:
        mouse_down = true;
        last_mouse_x = e->mouse_x;
        break;

    case RIZZ_APP_EVENTTYPE_TOUCHES_BEGAN:
        sx_assert(e->num_touches > 0);
        mouse_down = true;
        last_mouse_x = e->touches[0].pos_x;
        break;

    case RIZZ_APP_EVENTTYPE_TOUCHES_ENDED:
    case RIZZ_APP_EVENTTYPE_MOUSE_UP:
    case RIZZ_APP_EVENTTYPE_MOUSE_LEAVE:
        mouse_down = false;
        break;

    case RIZZ_APP_EVENTTYPE_TOUCHES_MOVED:
        sx_assert(e->num_touches > 0);
        float dx = sx_torad(e->touches[0].pos_x - last_mouse_x) * rotate_speed * dt;
        last_mouse_x = e->touches[0].pos_x;
        g_sdf.camera_orbit += dx;
        orbit_camera();
        break;

    case RIZZ_APP_EVENTTYPE_MOUSE_MOVE:
        if (mouse_down) {
            float dx = sx_torad(e->mouse_x - last_mouse_x) * rotate_speed * dt;
            last_mouse_x = e->mouse_x;
            g_sdf.camera_orbit += dx;
            orbit_camera();
        }
        break;

    default:
        break;
    }
}

rizz_game_decl_config(conf)
{
    conf->app_name = "sdf";
    conf->app_version = 1000;
    conf->app_title = "06 - SDF";
    conf->app_flags |= RIZZ_APP_FLAG_HIGHDPI;
    conf->window_width = EXAMPLES_DEFAULT_WIDTH;
    conf->window_height = EXAMPLES_DEFAULT_HEIGHT;
    conf->swap_interval = 2;
    conf->plugins[0] = "imgui";
}
