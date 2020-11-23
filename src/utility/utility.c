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
bool gradient__add_key(rizz_gradient* gradient, rizz_gradient_key key);
bool gradient__remove_key(rizz_gradient* gradient, int index);
bool gradient__move_key(rizz_gradient* gradient, int index, float t);
void gradient__eval(const rizz_gradient* gradient, float t, sx_color* outcolor);
void gradient__edit(const rizz_api_imgui* gui, const char* label, rizz_gradient* gradient);

void graph__init(rizz_graph* graph, float start, float end);
bool graph__add_key(rizz_graph* graph, rizz_graph_key key);
bool graph__remove_key(rizz_graph* graph, int index);
bool graph__move_key(rizz_graph* graph, int index, float t, float value);
float graph__eval(const rizz_graph* graph, float t);
float graph__eval_remap(const rizz_graph* graph, float t, float t_min, float t_max, float v_min,
                        float v_max);
void graph__edit(const rizz_api_imgui* gui, const char* label, rizz_graph* graph, sx_color color);
void graph__edit_multiple(const rizz_api_imgui* api, rizz_graph** graphs, const char** names,
                          sx_color* colors, int* selected, int num_graphs);

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

    .graph.init = graph__init,
    .graph.add_key = graph__add_key,
    .graph.remove_key = graph__remove_key,
    .graph.move_key = graph__move_key,
    .graph.eval = graph__eval,
    .graph.eval_remap = graph__eval_remap,
    .graph.edit = graph__edit,
    .graph.edit_multiple = graph__edit_multiple,
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