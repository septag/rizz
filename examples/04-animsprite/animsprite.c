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
#include "rizz/plugin.h"
#include "rizz/vfs.h"

#include "sprite/sprite.h"

#include "imgui/imgui-extra.h"
#include "imgui/imgui.h"

#include "../common.h"

#define MAX_VERTICES 1000
#define MAX_INDICES 2000
#define NUM_SPRITES 6
#define SPRITE_WIDTH 3.5f

RIZZ_STATE static rizz_api_core*        the_core;
RIZZ_STATE static rizz_api_gfx*         the_gfx;
RIZZ_STATE static rizz_api_app*         the_app;
RIZZ_STATE static rizz_api_imgui*       the_imgui;
RIZZ_STATE static rizz_api_asset*       the_asset;
RIZZ_STATE static rizz_api_imgui_extra* the_imguix;
RIZZ_STATE static rizz_api_camera*      the_camera;
RIZZ_STATE static rizz_api_vfs*         the_vfs;
RIZZ_STATE static rizz_api_sprite*      the_sprite;

typedef struct {
    rizz_gfx_stage       stage;
    rizz_asset           atlas;
    rizz_camera_fps      cam;
    rizz_sprite          sprite;
    rizz_sprite_animclip animclip;
} animsprite_state;

RIZZ_STATE static animsprite_state g_as;

static const rizz_sprite_animclip_frame_desc k_frames[] = {
    { .name = "test/boy_motion_idle_000.png" }, { .name = "test/boy_motion_idle_001.png" },
    { .name = "test/boy_motion_idle_002.png" }, { .name = "test/boy_motion_idle_003.png" },
    { .name = "test/boy_motion_idle_004.png" }, { .name = "test/boy_motion_idle_005.png" },
    { .name = "test/boy_motion_idle_006.png" }, { .name = "test/boy_motion_idle_007.png" }
};

static bool init() {
    // mount `/asset` directory
    char asset_dir[RIZZ_MAX_PATH];
    sx_os_path_join(asset_dir, sizeof(asset_dir), EXAMPLES_ROOT, "assets");    // "/examples/assets"
    the_vfs->mount(asset_dir, "/assets");
    the_vfs->watch_mounts();

    // load assets metadata cache to speedup asset loading
    // always do this after you have mounted all virtual directories
    the_asset->load_meta_cache();

    // register main graphics stage.
    // at least one stage should be registered if you want to draw anything
    g_as.stage = the_gfx->stage_register("main", (rizz_gfx_stage){ .id = 0 });
    sx_assert(g_as.stage.id);

    // camera
    // projection: setup for ortho, total-width = 10 units
    // view: Y-UP
    sx_vec2     screen_size = the_app->sizef();
    const float view_width = 5.0f;
    const float view_height = screen_size.y * view_width / screen_size.x;
    the_camera->fps_init(&g_as.cam, 50.0f,
                         sx_rectf(-view_width, -view_height, view_width, view_height), -5.0f, 5.0f);
    the_camera->fps_lookat(&g_as.cam, sx_vec3f(0, 0.0f, 1.0), SX_VEC3_ZERO, SX_VEC3_UNITY);

    // sprites and atlases
    rizz_atlas_load_params aparams = { .min_filter = SG_FILTER_LINEAR,
                                       .mag_filter = SG_FILTER_LINEAR };
    g_as.atlas = the_asset->load("atlas", "/assets/textures/boy.json", &aparams,
                                 RIZZ_ASSET_LOAD_FLAG_WAIT_ON_LOAD, NULL, 0);
    g_as.animclip = the_sprite->animclip_create(&(rizz_sprite_animclip_desc){
        .atlas = g_as.atlas,
        .frames = k_frames,
        .num_frames = sizeof(k_frames) / sizeof(rizz_sprite_animclip_frame_desc),
        .fps = 8.0f,
        .trigger_end_event = true });

    g_as.sprite = the_sprite->create(&(rizz_sprite_desc){ .name = "boy",
                                                          .clip = g_as.animclip,
                                                          .size = sx_vec2f(SPRITE_WIDTH, 0),
                                                          .color = sx_colorn(0xffffffff) });
    return true;
}

static void shutdown() {
    if (g_as.sprite.id)
        the_sprite->destroy(g_as.sprite);
    if (g_as.atlas.id)
        the_asset->unload(g_as.atlas);
    if (g_as.animclip.id)
        the_sprite->animclip_destroy(g_as.animclip);
}

static void update(float dt) {
    the_sprite->animclip_update(g_as.animclip, dt);
    rizz_event e;
    if (rizz_event_poll(the_sprite->animclip_events(g_as.animclip), &e)) {
        if (e.e == RIZZ_SPRITE_ANIMCLIP_EVENT_END) {
            rizz_log_debug(the_core, "END");
        }
    }
}

static void render() {
    sg_pass_action pass_action = { .colors[0] = { SG_ACTION_CLEAR, { 0.25f, 0.5f, 0.75f, 1.0f } },
                                   .depth = { SG_ACTION_CLEAR, 1.0f } };

    the_gfx->staged.begin(g_as.stage);
    the_gfx->staged.begin_default_pass(&pass_action, the_app->width(), the_app->height());

    // draw sprite
    sx_mat4 proj = the_camera->ortho_mat(&g_as.cam.cam);
    sx_mat4 view = the_camera->view_mat(&g_as.cam.cam, SX_VEC3_UNITY);
    sx_mat4 vp = sx_mat4_mul(&proj, &view);

    sx_mat3 mat = sx_mat3_ident();
    the_sprite->draw(g_as.sprite, &vp, &mat, SX_COLOR_WHITE, NULL);

    the_gfx->staged.end_pass();
    the_gfx->staged.end();

    // UI
    the_imgui->SetNextWindowContentSize(sx_vec2f(140.0f, 120.0f));
    if (the_imgui->Begin("animsprite", NULL, 0)) {
        the_imgui->LabelText("Fps", "%.3f", the_core->fps());
    }
    the_imgui->End();
}

rizz_plugin_decl_main(animsprite, plugin, e) {
    switch (e) {
    case RIZZ_PLUGIN_EVENT_STEP:
        update((float)sx_tm_sec(the_core->delta_tick()));
        render();
        break;

    case RIZZ_PLUGIN_EVENT_INIT:
        // runs only once for application. Retreive needed APIs
        the_core = plugin->api->get_api(RIZZ_API_CORE, 0);
        the_gfx = plugin->api->get_api(RIZZ_API_GFX, 0);
        the_app = plugin->api->get_api(RIZZ_API_APP, 0);
        the_imgui = plugin->api->get_api(RIZZ_API_IMGUI, 0);
        the_vfs = plugin->api->get_api(RIZZ_API_VFS, 0);
        the_asset = plugin->api->get_api(RIZZ_API_ASSET, 0);
        the_sprite = plugin->api->get_api(RIZZ_API_SPRITE, 0);
        the_camera = plugin->api->get_api(RIZZ_API_CAMERA, 0);
        the_imguix = plugin->api->get_api(RIZZ_API_IMGUI_EXTRA, 0);
        sx_assert(the_sprite && "sprite plugin is not loaded!");

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

rizz_plugin_decl_event_handler(animsprite, e) {
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

rizz_plugin_implement_info(animsprite, 1000, "animsprite", 0);

rizz_game_decl_config(conf) {
    conf->app_name = "animsprite";
    conf->app_version = 1000;
    conf->app_title = "03 - AnimSprite";
    conf->window_width = 800;
    conf->window_height = 600;
    conf->core_flags |= RIZZ_CORE_FLAG_VERBOSE;
    conf->multisample_count = 4;
    conf->swap_interval = 2;
}
