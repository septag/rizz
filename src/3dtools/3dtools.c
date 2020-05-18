#include "rizz/3dtools.h"

#include "sx/string.h"

#include "3dtools-internal.h"

RIZZ_STATE static rizz_api_plugin* the_plugin;

static rizz_api_prims3d the__prims3d = {
    .draw_box = prims3d__draw_box,
    .draw_boxes = prims3d__draw_boxes,
    .draw_aabb = prims3d__draw_aabb,
    .draw_aabbs = prims3d__draw_aabbs
};

rizz_plugin_decl_main(3dtools, plugin, e)
{
    switch (e) {
    case RIZZ_PLUGIN_EVENT_STEP:
        break;

    case RIZZ_PLUGIN_EVENT_INIT: {
        the_plugin = plugin->api;
        the_plugin->inject_api("prims3d", 0, &the__prims3d);
    } break;

    case RIZZ_PLUGIN_EVENT_LOAD:
        break;

    case RIZZ_PLUGIN_EVENT_UNLOAD:
        break;

    case RIZZ_PLUGIN_EVENT_SHUTDOWN:
        the_plugin->remove_api("prims3d", 0);
        break;
    }

    return 0;
}

rizz_plugin_decl_event_handler(2dtools, e) 
{
    if (e->type == RIZZ_APP_EVENTTYPE_UPDATE_APIS) {

    }
}

static const char* tools3d__deps[] = { "imgui" };
rizz_plugin_implement_info(3dtools, 1000, "3dtools plugin", tools3d__deps, 1);