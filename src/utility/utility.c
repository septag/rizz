#include "rizz/utility.h"
#include "sx/string.h"

RIZZ_STATE static rizz_api_plugin* the_plugin;

void spline__eval2d(const rizz_spline2d_desc* desc, sx_vec2* result);
void spline__eval3d(const rizz_spline3d_desc* desc, sx_vec3* result);

float noise__perlin1d(float x);
float noise__perlin2d(float x, float y);
float noise__perlin1d_fbm(float x, int octave);
float noise__perlin2d_fbm(float x, float y, int octave);

void gradient__init(rizz_gradient* gradient, sx_color start, sx_color end);
bool gradient__add_key(rizz_gradient* gradient, sx_color color, float t);
bool gradient__remove_key(rizz_gradient* gradient, int index);
bool gradient__move_key(rizz_gradient* gradient, int index, float t);
sx_color gradient__eval(const rizz_gradient* gradient, float t);
void gradient__edit(const rizz_api_imgui* gui, const char* label, rizz_gradient* gradient);


static rizz_api_utility the__utility = {
    .spline.eval2d = spline__eval2d,
    .spline.eval3d = spline__eval3d,
    .noise.perlin1d = noise__perlin1d,
    .noise.perlin2d = noise__perlin2d,
    .noise.perlin1d_fbm = noise__perlin1d_fbm,
    .noise.perlin2d_fbm = noise__perlin2d_fbm,
    .gradient.init = gradient__init,
    .gradient.add_key = gradient__add_key,
    .gradient.remove_key = gradient__remove_key,
    .gradient.move_key = gradient__move_key,
    .gradient.eval = gradient__eval,
    .gradient.edit = gradient__edit,
};

rizz_plugin_decl_main(utility, plugin, e)
{
    switch (e) {
    case RIZZ_PLUGIN_EVENT_STEP:
        break;
    case RIZZ_PLUGIN_EVENT_INIT:
        the_plugin = plugin->api;

        the_plugin->inject_api("utility", 0, &the__utility);
        break;
    case RIZZ_PLUGIN_EVENT_LOAD:
        the_plugin->inject_api("utility", 0, &the__utility);
        break;
    case RIZZ_PLUGIN_EVENT_UNLOAD:
        break;
    case RIZZ_PLUGIN_EVENT_SHUTDOWN:
        the_plugin->remove_api("utility", 0);
        break;
    }

    return 0;
}

rizz_plugin_implement_info(utility, 1000, "utility plugin", NULL, 0);