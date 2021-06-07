#include "sx/os.h"
#include "sx/string.h"
#include "sx/timer.h"
#include "sx/math-vec.h"

#include "rizz/imgui-extra.h"
#include "rizz/imgui.h"
#include "rizz/2dtools.h"
#include "rizz/rizz.h"

#include "../common.h"

#define MAX_VERTICES 1000
#define MAX_INDICES 2000
#define NUM_SPRITES 6
#define SPRITE_WIDTH 3.5f

RIZZ_STATE static rizz_api_plugin* the_plugin;
RIZZ_STATE static rizz_api_core* the_core;
RIZZ_STATE static rizz_api_gfx* the_gfx;
RIZZ_STATE static rizz_api_app* the_app;
RIZZ_STATE static rizz_api_imgui* the_imgui;
RIZZ_STATE static rizz_api_asset* the_asset;
RIZZ_STATE static rizz_api_imgui_extra* the_imguix;
RIZZ_STATE static rizz_api_camera* the_camera;
RIZZ_STATE static rizz_api_vfs* the_vfs;
RIZZ_STATE static rizz_api_2d* the_2d;

typedef struct {
    rizz_gfx_stage stage;
    rizz_asset atlas;
    rizz_camera_fps cam;
    rizz_sprite sprite;
    rizz_sprite_animclip animclips[5];
    rizz_sprite_animctrl animctrl;
} animsprite_state;

RIZZ_STATE static animsprite_state g_as;

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
    g_as.stage = the_gfx->stage_register("main", (rizz_gfx_stage){ .id = 0 });
    sx_assert(g_as.stage.id);

    // camera
    // projection: setup for ortho, total-width = 10 units
    // view: Y-UP
    sx_vec2 screen_size;
    the_app->window_size(&screen_size);
    const float view_width = 5.0f;
    const float view_height = screen_size.y * view_width / screen_size.x;
    the_camera->fps_init(&g_as.cam, 50.0f,
                         sx_rectf(-view_width, -view_height, view_width, view_height), -5.0f, 5.0f);
    the_camera->fps_lookat(&g_as.cam, sx_vec3f(0, 0.0f, 1.0), SX_VEC3_ZERO, SX_VEC3_UNITY);

    static const rizz_sprite_animclip_frame_desc k_idle_frames[] = {
        { .name = "test/boy_motion_idle_000.png" }, { .name = "test/boy_motion_idle_001.png" },
        { .name = "test/boy_motion_idle_002.png" }, { .name = "test/boy_motion_idle_003.png" },
        { .name = "test/boy_motion_idle_004.png" }, { .name = "test/boy_motion_idle_005.png" },
        { .name = "test/boy_motion_idle_006.png" }, { .name = "test/boy_motion_idle_007.png" }
    };

    static const rizz_sprite_animclip_frame_desc k_walk_frames[] = {
        { .name = "test/boy_motion_walk_000.png" }, { .name = "test/boy_motion_walk_001.png" },
        { .name = "test/boy_motion_walk_002.png" }, { .name = "test/boy_motion_walk_003.png" },
        { .name = "test/boy_motion_walk_004.png" }, { .name = "test/boy_motion_walk_005.png" },
        { .name = "test/boy_motion_walk_006.png" }
    };

    static const rizz_sprite_animclip_frame_desc k_jumpstart_frames[] = {
        { .name = "test/boy_motion_jump_start_000.png" },
        { .name = "test/boy_motion_jump_start_001.png" },
        { .name = "test/boy_motion_jump_start_002.png" },
        { .name = "test/boy_motion_jump_start_003.png" },
        { .name = "test/boy_motion_jump_start_004.png" },
        { .name = "test/boy_motion_jump_start_005.png" }
    };

    static const rizz_sprite_animclip_frame_desc k_jumploop_frames[] = {
        { .name = "test/boy_motion_jump_loop_000.png" },
        { .name = "test/boy_motion_jump_loop_001.png" },
        { .name = "test/boy_motion_jump_loop_002.png" },
        { .name = "test/boy_motion_jump_loop_003.png" }
    };

    static const rizz_sprite_animclip_frame_desc k_jumpend_frames[] = {
        { .name = "test/boy_motion_jump_end_000.png" },
        { .name = "test/boy_motion_jump_end_001.png" },
        { .name = "test/boy_motion_jump_end_002.png" }
    };

    // sprites and atlases
    rizz_atlas_load_params aparams = { .min_filter = SG_FILTER_LINEAR,
                                       .mag_filter = SG_FILTER_LINEAR };
    g_as.atlas = the_asset->load("atlas", "/assets/textures/boy.json", &aparams,
                                 RIZZ_ASSET_LOAD_FLAG_WAIT_ON_LOAD, NULL, 0);
    g_as.animclips[0] = the_2d->sprite.animclip_create(&(rizz_sprite_animclip_desc){
        .atlas = g_as.atlas,
        .frames = k_idle_frames,
        .num_frames = sizeof(k_idle_frames) / sizeof(rizz_sprite_animclip_frame_desc),
        .fps = 8.0f });
    g_as.animclips[1] = the_2d->sprite.animclip_create(&(rizz_sprite_animclip_desc){
        .atlas = g_as.atlas,
        .frames = k_walk_frames,
        .num_frames = sizeof(k_walk_frames) / sizeof(rizz_sprite_animclip_frame_desc),
        .fps = 8.0f,
    });
    g_as.animclips[2] = the_2d->sprite.animclip_create(&(rizz_sprite_animclip_desc){
        .atlas = g_as.atlas,
        .frames = k_jumpstart_frames,
        .num_frames = sizeof(k_jumpstart_frames) / sizeof(rizz_sprite_animclip_frame_desc),
        .fps = 12.0f,
    });
    g_as.animclips[3] = the_2d->sprite.animclip_create(&(rizz_sprite_animclip_desc){
        .atlas = g_as.atlas,
        .frames = k_jumploop_frames,
        .num_frames = sizeof(k_jumploop_frames) / sizeof(rizz_sprite_animclip_frame_desc),
        .fps = 12.0f,
    });
    g_as.animclips[4] = the_2d->sprite.animclip_create(&(rizz_sprite_animclip_desc){
        .atlas = g_as.atlas,
        .frames = k_jumpend_frames,
        .num_frames = sizeof(k_jumpend_frames) / sizeof(rizz_sprite_animclip_frame_desc),
        .fps = 12.0f,
    });

    const rizz_sprite_animctrl_state_desc k_states[] = {
        { .name = "idle", .clip = g_as.animclips[0] },
        { .name = "walk", .clip = g_as.animclips[1] },
        { .name = "jump_start", .clip = g_as.animclips[2] },
        { .name = "jump_loop", .clip = g_as.animclips[3] },
        { .name = "jump_end", .clip = g_as.animclips[4] }
    };

    const rizz_sprite_animctrl_transition_desc k_transitions[] = {
        { .state = "idle",
          .target_state = "walk",
          .trigger = { .param_name = "walk",
                       .func = RIZZ_SPRITE_COMPAREFUNC_EQUAL,
                       .value.b = true } },
        { .state = "walk",
          .target_state = "idle",
          .trigger = { .param_name = "idle",
                       .func = RIZZ_SPRITE_COMPAREFUNC_EQUAL,
                       .value.b = true } },
        // jump from walk or idle mode
        { .state = "idle",
          .target_state = "jump_start",
          .trigger = { .param_name = "jump",
                       .func = RIZZ_SPRITE_COMPAREFUNC_EQUAL,
                       .value.b = true } },
        { .state = "walk",
          .target_state = "jump_start",
          .trigger = { .param_name = "jump",
                       .func = RIZZ_SPRITE_COMPAREFUNC_EQUAL,
                       .value.b = true } },
        // jump_start -> jump_loop
        { .state = "jump_start", .target_state = "jump_loop" },
        { .state = "jump_loop",
          .target_state = "jump_end",
          .trigger = { .param_name = "land",
                       .func = RIZZ_SPRITE_COMPAREFUNC_EQUAL,
                       .value.b = true } },
        { .state = "jump_end", .target_state = "idle" },
    };

    g_as.animctrl = the_2d->sprite.animctrl_create(&(rizz_sprite_animctrl_desc){
        .states = k_states,
        .start_state = "idle",
        .num_states = sizeof(k_states) / sizeof(rizz_sprite_animctrl_state_desc),
        .transitions = k_transitions,
        .num_transitions = sizeof(k_transitions) / sizeof(rizz_sprite_animctrl_transition_desc),
        .params[0] = { .name = "walk", .type = RIZZ_SPRITE_PARAMTYPE_BOOL_AUTO },
        .params[1] = { .name = "idle", .type = RIZZ_SPRITE_PARAMTYPE_BOOL_AUTO },
        .params[2] = { .name = "jump", .type = RIZZ_SPRITE_PARAMTYPE_BOOL_AUTO },
        .params[3] = { .name = "land", .type = RIZZ_SPRITE_PARAMTYPE_BOOL_AUTO } });

    g_as.sprite = the_2d->sprite.create(&(rizz_sprite_desc){ .name = "boy",
                                                          .ctrl = g_as.animctrl,
                                                          .size = sx_vec2f(SPRITE_WIDTH, 0),
                                                          .color = sx_colorn(0xffffffff) });
    return true;
}

static void shutdown()
{
    if (g_as.sprite.id)
        the_2d->sprite.destroy(g_as.sprite);
    if (g_as.atlas.id)
        the_asset->unload(g_as.atlas);
    for (int i = 0; i < 5; i++)
        the_2d->sprite.animclip_destroy(g_as.animclips[i]);
    if (g_as.animctrl.id)
        the_2d->sprite.animctrl_destroy(g_as.animctrl);
}

static void update(float dt)
{
    the_2d->sprite.animctrl_update(g_as.animctrl, dt);
}

static void render()
{
    sg_pass_action pass_action = { .colors[0] = { SG_ACTION_CLEAR, { 0.25f, 0.5f, 0.75f, 1.0f } },
                                   .depth = { SG_ACTION_CLEAR, 1.0f } };

    the_gfx->staged.begin(g_as.stage);
    the_gfx->staged.begin_default_pass(&pass_action, the_app->width(), the_app->height());

    // draw sprite
    sx_mat4 proj, view;
    the_camera->ortho_mat(&g_as.cam.cam, &proj);
    the_camera->view_mat(&g_as.cam.cam, &view);
    sx_mat4 vp = sx_mat4_mul(&proj, &view);

    sx_mat3 mat = sx_mat3_ident();
    the_2d->sprite.draw(g_as.sprite, &vp, &mat, SX_COLOR_WHITE);

    the_gfx->staged.end_pass();
    the_gfx->staged.end();

    // UI
    show_debugmenu(the_imgui, the_core);
    
    static bool show_debugger = false;
    the_imgui->SetNextWindowContentSize(sx_vec2f(140.0f, 120.0f));
    if (the_imgui->Begin("animsprite", NULL, 0)) {
        the_imgui->LabelText("Fps", "%.3f", the_core->fps());
        the_imgui->Checkbox("Show Debugger", &show_debugger);
    }
    the_imgui->End();

    if (show_debugger)
        the_2d->sprite.show_debugger(&show_debugger);
}

rizz_plugin_decl_main(animsprite, plugin, e)
{
    switch (e) {
    case RIZZ_PLUGIN_EVENT_STEP:
        update(the_core->delta_time());
        render();
        break;

    case RIZZ_PLUGIN_EVENT_INIT:
        // runs only once for application. Retreive needed APIs
        the_plugin = plugin->api;
        the_core = the_plugin->get_api(RIZZ_API_CORE, 0);
        the_gfx = the_plugin->get_api(RIZZ_API_GFX, 0);
        the_app = the_plugin->get_api(RIZZ_API_APP, 0);
        the_vfs = the_plugin->get_api(RIZZ_API_VFS, 0);
        the_asset = the_plugin->get_api(RIZZ_API_ASSET, 0);
        the_camera = the_plugin->get_api(RIZZ_API_CAMERA, 0);

        the_imgui = the_plugin->get_api_byname("imgui", 0);
        the_imguix = the_plugin->get_api_byname("imgui_extra", 0);
        the_2d = the_plugin->get_api_byname("2dtools", 0);

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

rizz_plugin_decl_event_handler(animsprite, e)
{
    switch (e->type) {
    case RIZZ_APP_EVENTTYPE_UPDATE_APIS:
        the_imgui = the_plugin->get_api_byname("imgui", 0);
        the_imguix = the_plugin->get_api_byname("imgui_extra", 0);
        the_2d = the_plugin->get_api_byname("2dtools", 0);
        break;
    default:
        break;
    }
}

rizz_game_decl_config(conf)
{
    conf->app_name = "animsprite";
    conf->app_version = 1000;
    conf->app_title = "03 - AnimSprite";
    conf->app_flags |= RIZZ_APP_FLAG_HIGHDPI;
    conf->window_width = EXAMPLES_DEFAULT_WIDTH;
    conf->window_height = EXAMPLES_DEFAULT_HEIGHT;
    conf->swap_interval = 2;
    conf->plugins[0] = "imgui";
    conf->plugins[1] = "2dtools";
}
