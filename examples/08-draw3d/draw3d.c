#include "sx/math.h"
#include "sx/os.h"
#include "sx/string.h"

#include "rizz/imgui.h"
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

typedef struct {
    rizz_gfx_stage stage;
    rizz_camera_fps cam;
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
    
    sx_vec3 frustum[8];
    the_camera->calc_frustum_points_range(&g_draw3d.cam.cam, frustum, -5.0f, 50.0f);
    the_gfx->debug_grid_xyplane(1.0f, 5.0f, &viewproj, frustum);
    the_prims->draw_boxes(boxes, 100, &viewproj, RIZZ_PRIMS3D_MAPTYPE_CHECKER, tints);

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

            the_camera->fps_pitch(&g_draw3d.cam, dy);
            the_camera->fps_yaw(&g_draw3d.cam, dx);
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
    conf->window_width = 800;
    conf->window_height = 600;
    conf->multisample_count = 4;
    conf->swap_interval = 1;
    conf->plugins[0] = "imgui";
    conf->plugins[1] = "3dtools";
}