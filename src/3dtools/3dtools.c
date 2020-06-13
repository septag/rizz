#include "rizz/3dtools.h"

#include "rizz/rizz.h"
#include "sx/string.h"

#include "3dtools-internal.h"

RIZZ_STATE static rizz_api_plugin* the_plugin;

static rizz_api_prims3d the__prims3d = {
    .draw_box = prims3d__draw_box,
    .draw_boxes = prims3d__draw_boxes,
    .generate_box_geometry = prims3d__generate_box_geometry,
    .free_geometry = prims3d__free_geometry,
    .draw_aabb = prims3d__draw_aabb,
    .draw_aabbs = prims3d__draw_aabbs,
    .grid_xyplane = prims3d__grid_xyplane,
    .grid_xzplane = prims3d__grid_xzplane,
    .grid_xyplane_cam = prims3d__grid_xyplane_cam
};

static rizz_api_model the__model = {
    .model_get = model__get,
    .material_get = model__get_material
};

rizz_plugin_decl_main(3dtools, plugin, e)
{
    switch (e) {
    case RIZZ_PLUGIN_EVENT_STEP:
        break;

    case RIZZ_PLUGIN_EVENT_INIT: {
        the_plugin = plugin->api;
        rizz_api_core* core = the_plugin->get_api(RIZZ_API_CORE, 0);
        rizz_api_gfx* gfx = the_plugin->get_api(RIZZ_API_GFX, 0);
        rizz_api_camera* cam = the_plugin->get_api(RIZZ_API_CAMERA, 0);
        rizz_api_asset* asset = the_plugin->get_api(RIZZ_API_ASSET, 0);
        rizz_api_imgui* imgui = the_plugin->get_api_byname("imgui", 0);

        if (!prims3d__init(core, gfx, cam)) {
            return -1;
        }
        the_plugin->inject_api("prims3d", 0, &the__prims3d);

        if (!model__init(core, asset, gfx, imgui)) {
            return -1;
        }
        the_plugin->inject_api("model", 0, &the__model);

    } break;

    case RIZZ_PLUGIN_EVENT_LOAD:
        break;

    case RIZZ_PLUGIN_EVENT_UNLOAD:
        break;

    case RIZZ_PLUGIN_EVENT_SHUTDOWN:
        the_plugin->remove_api("prims3d", 0);
        the_plugin->remove_api("model", 0);
        prims3d__release();
        model__release();
        break;
    }

    return 0;
}

rizz_plugin_decl_event_handler(2dtools, e) 
{
    if (e->type == RIZZ_APP_EVENTTYPE_UPDATE_APIS) {
        rizz_api_imgui* imgui = the_plugin->get_api_byname("imgui", 0);
        model__set_imgui(imgui);
    }
}

static const char* tools3d__deps[] = { "imgui" };
rizz_plugin_implement_info(3dtools, 1000, "3dtools plugin", tools3d__deps, 1);