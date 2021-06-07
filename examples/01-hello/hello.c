#include "sx/string.h"
#include "sx/timer.h"

#include "rizz/rizz.h"
#include "rizz/imgui-extra.h"
#include "rizz/imgui.h"

#include "../common.h"

RIZZ_STATE static rizz_api_core* the_core;
RIZZ_STATE static rizz_api_gfx* the_gfx;
RIZZ_STATE static rizz_api_app* the_app;
RIZZ_STATE static rizz_api_imgui* the_imgui;

RIZZ_STATE rizz_gfx_stage g_stage;

static bool init()
{
    // register main graphics stage.
    // at least one stage should be registered if you want to draw anything
    g_stage = the_gfx->stage_register("main", (rizz_gfx_stage){ .id = 0 });
    sx_assert(g_stage.id);

    return true;
}

static void shutdown() {}

static void update(float dt) {}

static void render()
{
    sg_pass_action pass_action = { .colors[0] = { SG_ACTION_CLEAR, { 0.25f, 0.5f, 0.75f, 1.0f } },
                                   .depth = { SG_ACTION_DONTCARE, 1.0f } };
    
    the_gfx->staged.begin(g_stage);
    the_gfx->staged.begin_default_pass(&pass_action, the_app->width(), the_app->height());
    the_gfx->staged.end_pass();
    the_gfx->staged.end();

    // Use imgui UI
    show_debugmenu(the_imgui, the_core);

    if (the_imgui) {
        the_imgui->SetNextWindowContentSize(sx_vec2f(100.0f, 50.0f));
        if (the_imgui->Begin("Hello", NULL, 0)) {
            the_imgui->LabelText("Fps", "%.3f", the_core->fps());
        }
        the_imgui->End();
    }
}

rizz_plugin_decl_main(hello, plugin, e)
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

rizz_plugin_decl_event_handler(hello, e)
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
    conf->app_name = "hello";
    conf->app_version = 1000;
    conf->app_title = "01 - Hello";
    conf->app_flags |= RIZZ_APP_FLAG_HIGHDPI;
    conf->window_width = EXAMPLES_DEFAULT_WIDTH;
    conf->window_height = EXAMPLES_DEFAULT_HEIGHT;
    conf->swap_interval = 2;
    conf->plugins[0] = "imgui";
}
