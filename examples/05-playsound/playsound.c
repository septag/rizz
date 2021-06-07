#include "sx/os.h"
#include "sx/string.h"
#include "sx/timer.h"
#include "sx/math.h"

#include "rizz/imgui-extra.h"
#include "rizz/imgui.h"
#include "rizz/rizz.h"
#include "rizz/sound.h"

#include "../common.h"

RIZZ_STATE static rizz_api_core* the_core;
RIZZ_STATE static rizz_api_gfx* the_gfx;
RIZZ_STATE static rizz_api_app* the_app;
RIZZ_STATE static rizz_api_imgui* the_imgui;
RIZZ_STATE static rizz_api_asset* the_asset;
RIZZ_STATE static rizz_api_vfs* the_vfs;
RIZZ_STATE static rizz_api_snd* the_sound;

RIZZ_STATE rizz_gfx_stage g_stage;
RIZZ_STATE rizz_asset g_snd[8];

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
    g_stage = the_gfx->stage_register("main", (rizz_gfx_stage){ .id = 0 });
    sx_assert(g_stage.id);

    rizz_snd_load_params sparams = { .volume = 1.0f };
    for (int i = 0; i < 8; i++) {
        char file[128];
        sx_snprintf(file, sizeof(file), "/assets/sounds/fish_combo_%d.wav", i + 1);
        g_snd[i] = the_asset->load("sound", file, &sparams, 0, NULL, 0);
    }

    return true;
}

static void shutdown()
{
    for (int i = 0; i < 8; i++) {
        if (g_snd[i].id) {
            the_asset->unload(g_snd[i]);
        }
    }
}

static void update(float dt)
{
    // Use imgui UI
    show_debugmenu(the_imgui, the_core);

    the_imgui->SetNextWindowContentSize(sx_vec2f(200.0f, 150.0f));
    if (the_imgui->Begin("PlaySound", NULL, 0)) {
        the_imgui->LabelText("Fps", "%.3f", the_core->fps());
        the_imgui->Separator();
        the_imgui->Text("Click colored buttons to play sounds");
        float hsv[3] = { 0.5f, 1.0f, 1.0f };
        for (int i = 0; i < 8; i++) {
            char btn_id[32];
            sx_snprintf(btn_id, sizeof(btn_id), "sound #%d", i + 1);

            sx_vec4 color = sx_vec4f(0, 0, 0, 1.0f);
            sx_color_HSVtoRGB(color.f, hsv);

            if (the_imgui->ColorButton(btn_id, color, ImGuiColorEditFlags_NoTooltip,
                                       sx_vec2f(30.0f, 30.0f))) {
                the_sound->play(the_sound->source_get(g_snd[i]), 0, 1.0f, 0, false);
            }
            if ((i + 1) % 4 != 0) {
                the_imgui->SameLine(0, -1.0f);
            }

            hsv[0] = sx_fract(hsv[0] + 0.618033988749895f);
        }
    }
    the_imgui->End();

    the_sound->show_debugger(NULL);
}

static void render()
{
    sg_pass_action pass_action = { .colors[0] = { SG_ACTION_CLEAR, { 0.25f, 0.5f, 0.75f, 1.0f } },
                                   .depth = { SG_ACTION_CLEAR, 1.0f } };

    the_gfx->staged.begin(g_stage);
    the_gfx->staged.begin_default_pass(&pass_action, the_app->width(), the_app->height());
    the_gfx->staged.end_pass();
    the_gfx->staged.end();
}

rizz_plugin_decl_main(playsound, plugin, e)
{
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
        the_imgui = plugin->api->get_api_byname("imgui", 0);
        the_vfs = plugin->api->get_api(RIZZ_API_VFS, 0);
        the_asset = plugin->api->get_api(RIZZ_API_ASSET, 0);
        the_sound = plugin->api->get_api_byname("sound", 0);
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

rizz_plugin_decl_event_handler(playsound, e)
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
    conf->app_name = "playsound";
    conf->app_version = 1000;
    conf->app_title = "05 - PlaySound";
    conf->app_flags |= RIZZ_APP_FLAG_HIGHDPI;
    conf->window_width = EXAMPLES_DEFAULT_WIDTH;
    conf->window_height = EXAMPLES_DEFAULT_HEIGHT;
    conf->swap_interval = 1;
    conf->plugins[0] = "imgui";
    conf->plugins[1] = "sound";
}
