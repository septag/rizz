#include "rizz/2dtools.h"

#include "rizz/imgui-extra.h"
#include "rizz/imgui.h"
#include "rizz/rizz.h"

#include "sx/string.h"

#include "2dtools-internal.h"

RIZZ_STATE static rizz_api_plugin* the_plugin;

static rizz_api_2d the__2d = {
    .font = { 
        .get = font__get,
        .draw = font__draw,
        .drawf = font__drawf,
        .draw_debug = font__draw_debug,
        .push_state = font__push_state,
        .pop_state = font__pop_state,
        .set_size = font__set_size,
        .set_color = font__set_color,
        .set_align = font__set_align,
        .set_spacing = font__set_spacing,
        .set_blur = font__set_blur,
        .set_scissor = font__set_scissor,
        .set_viewproj_mat = font__set_viewproj_mat,
        .clear_state = font__clear_state,
        .bounds = font__bounds,
        .line_bounds = font__line_bounds,
        .vert_metrics = font__vert_metrics,
        .resize_draw_limits = font__resize_draw_limits,
        .set_draw_api = font__set_draw_api,
        .iter_init = font__iter_init,
        .iter_next = font__iter_next },
    .sprite = { 
        .create = sprite__create,
        .destroy = sprite__destroy,
        .clone = sprite__clone,
        .atlas_get = sprite__atlas_get,
        .size = sprite__size,
        .origin = sprite__origin,
        .bounds = sprite__bounds,
        .draw_bounds = sprite__draw_bounds,
        .flip = sprite__flip,
        .name = sprite__name,
        .set_size = sprite__set_size,
        .set_origin = sprite__set_origin,
        .set_color = sprite__set_color,
        .set_flip = sprite__set_flip,
        .make_drawdata = sprite__drawdata_make,
        .make_drawdata_batch = sprite__drawdata_make_batch,
        .free_drawdata = sprite__drawdata_free,
        .draw = sprite__draw,
        .draw_batch = sprite__draw_batch,
        .draw_srt = sprite__draw_srt,
        .draw_batch_srt = sprite__draw_batch_srt,
        .draw_wireframe_batch = sprite__draw_wireframe_batch,
        .resize_draw_limits = sprite__resize_draw_limits,
        .set_draw_api = sprite__set_draw_api,
        .animclip_create = sprite__animclip_create,
        .animclip_destroy = sprite__animclip_destroy,
        .animclip_clone = sprite__animclip_clone,
        .animclip_update = sprite__animclip_update,
        .animclip_update_batch = sprite__animclip_update_batch,
        .animclip_fps = sprite__animclip_fps,
        .animclip_len = sprite__animclip_len,
        .animclip_events = sprite__animclip_events,
        .animclip_set_fps = sprite__animclip_set_fps,
        .animclip_set_len = sprite__animclip_set_len,
        .animclip_restart = sprite__animclip_restart,
        .animctrl_create = sprite__animctrl_create,
        .animctrl_destroy = sprite__animctrl_destroy,
        .animctrl_update = sprite__animctrl_update,
        .animctrl_update_batch = sprite__animctrl_update_batch,
        .animctrl_set_paramb = sprite__animctrl_set_paramb,
        .animctrl_set_parami = sprite__animctrl_set_parami,
        .animctrl_set_paramf = sprite__animctrl_set_paramf,
        .animctrl_param_valueb = sprite__animctrl_param_valueb,
        .animctrl_param_valuei = sprite__animctrl_param_valuei,
        .animctrl_param_valuef = sprite__animctrl_param_valuef,
        .animctrl_restart = sprite__animctrl_restart,
        .show_debugger = sprite__show_debugger
    }
};

rizz_plugin_decl_main(2dtools, plugin, e)
{
    switch (e) {
    case RIZZ_PLUGIN_EVENT_STEP:
        font__update();
        break;

    case RIZZ_PLUGIN_EVENT_INIT: {
        the_plugin = plugin->api;
        rizz_api_core* core = the_plugin->get_api(RIZZ_API_CORE, 0);
        rizz_api_asset* asset = the_plugin->get_api(RIZZ_API_ASSET, 0);
        rizz_api_gfx* gfx = the_plugin->get_api(RIZZ_API_GFX, 0);
        rizz_api_app* app = the_plugin->get_api(RIZZ_API_APP, 0);
        rizz_api_imgui* imgui = the_plugin->get_api_byname("imgui", 0);
        if (!sprite__init(core, asset, gfx) || !font__init(core, asset, gfx, app)) {
            return -1;
        }
        sprite__set_imgui(imgui);
        the_plugin->inject_api("2dtools", 0, &the__2d);
    } break;

    case RIZZ_PLUGIN_EVENT_LOAD:
        the_plugin->inject_api("2dtools", 0, &the__2d);
        break;

    case RIZZ_PLUGIN_EVENT_UNLOAD:
        break;

    case RIZZ_PLUGIN_EVENT_SHUTDOWN:
        the_plugin->remove_api("2dtools", 0);
        sprite__release();
        font__release();
        break;
    }

    return 0;
}

rizz_plugin_decl_event_handler(2dtools, e)
{
    if (e->type == RIZZ_APP_EVENTTYPE_UPDATE_APIS) {
        rizz_api_imgui* imgui = the_plugin->get_api_byname("imgui", 0);
        sprite__set_imgui(imgui);
        font__set_imgui(imgui);
    }
}

static const char* tools2d__deps[] = { "imgui" };
rizz_plugin_implement_info(2dtools, 1000, "2dtools plugin", tools2d__deps, 1);
