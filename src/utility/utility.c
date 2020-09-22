#include "rizz/noise.h"
#include "rizz/spline.h"
#include "sx/string.h"

RIZZ_STATE static rizz_api_plugin* the_plugin;

// ------- spline api --------
void spline__eval2d(const rizz_spline2d_desc* desc, sx_vec2* result);
void spline__eval3d(const rizz_spline3d_desc* desc, sx_vec3* result);
static rizz_api_spline the__spline = (rizz_api_spline){
    .eval2d = spline__eval2d,
    .eval3d = spline__eval3d,
};

// ------- noise api --------
float noise__perlin1d(float x);
float noise__perlin2d(float x, float y);
float noise__perlin1d_fbm(float x, int octave);
float noise__perlin2d_fbm(float x, float y, int octave);
static rizz_api_noise the__noise = (rizz_api_noise){
    .perlin1d = noise__perlin1d,
    .perlin2d = noise__perlin2d,
    .perlin1d_fbm = noise__perlin1d_fbm,
    .perlin2d_fbm = noise__perlin2d_fbm,
};

rizz_plugin_decl_main(utility, plugin, e)
{
    switch (e) {
    case RIZZ_PLUGIN_EVENT_STEP:
        break;
    case RIZZ_PLUGIN_EVENT_INIT:
        the_plugin = plugin->api;

        the_plugin->inject_api("spline", 0, &the__spline);
        the_plugin->inject_api("noise", 0, &the__noise);
        break;
    case RIZZ_PLUGIN_EVENT_LOAD:
        the_plugin->inject_api("spline", 0, &the__spline);
        the_plugin->inject_api("noise", 0, &the__noise);
        break;
    case RIZZ_PLUGIN_EVENT_UNLOAD:
        break;
    case RIZZ_PLUGIN_EVENT_SHUTDOWN:
        the_plugin->remove_api("spline", 0);
        the_plugin->remove_api("noise", 0);
        break;
    }

    return 0;
}

rizz_plugin_implement_info(utility, 1000, "utility plugin", NULL, 0);