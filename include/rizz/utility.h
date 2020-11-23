#pragma once
#include "rizz/rizz.h"

typedef struct rizz_api_imgui rizz_api_imgui;

#define RIZZ_SPLINE3D_NODE_FIELDS \
    struct {                      \
        sx_vec3 pos;              \
        sx_vec3 lwing;            \
        sx_vec3 rwing;            \
    };

#define RIZZ_SPLINE2D_NODE_FIELDS \
    struct {                      \
        sx_vec2 pos;              \
        sx_vec2 lwing;            \
        sx_vec2 rwing;            \
    };

typedef struct rizz_spline3d_node {
    RIZZ_SPLINE3D_NODE_FIELDS
} rizz_spline3d_node;

typedef struct rizz_spline2d_node {
    RIZZ_SPLINE2D_NODE_FIELDS
} rizz_spline2d_node;

typedef struct rizz_spline2d_desc {
    const rizz_spline2d_node* nodes;
    uint32_t num_nodes;
    uint32_t node_stride;
    float time;
    void (*usereval)(const rizz_spline2d_node* n1, const rizz_spline2d_node* n2, float t,
                     sx_vec2* result);
    bool norm, loop;
} rizz_spline2d_desc;

typedef struct rizz_spline3d_desc {
    const rizz_spline3d_node* nodes;
    uint32_t num_nodes;
    uint32_t node_stride;
    float time;
    void (*usereval)(const rizz_spline3d_node* n1, const rizz_spline3d_node* n2, float t,
                     sx_vec3* result);
    bool norm, loop;
} rizz_spline3d_desc;

#define RIZZ_GRADIENT_MAX_KEYS 8

typedef struct rizz_gradient_key {
    float t;
    sx_color color;
} rizz_gradient_key;

typedef struct rizz_gradient {
    rizz_gradient_key keys[RIZZ_GRADIENT_MAX_KEYS];
    uint32_t num_keys;
} rizz_gradient;

#define RIZZ_GRAPH_MAX_KEYS 8

typedef struct rizz_graph_key {
    float t;
    float value;
    float lwing;
    float rwing;
} rizz_graph_key;

typedef struct rizz_graph {
    rizz_graph_key keys[RIZZ_GRAPH_MAX_KEYS];
    uint32_t num_keys;
} rizz_graph;

typedef struct rizz_api_utility {
    struct {
        void (*eval2d)(const rizz_spline2d_desc* desc, sx_vec2* result);
        void (*eval3d)(const rizz_spline3d_desc* desc, sx_vec3* result);
    } spline;
    struct {
        float (*perlin1d)(float x);
        float (*perlin2d)(float x, float y);
        float (*perlin1d_fbm)(float x, int octave);
        float (*perlin2d_fbm)(float x, float y, int octave);
    } noise;
    struct {
        void (*init)(rizz_gradient* gradient, sx_color start, sx_color end);
        bool (*add_key)(rizz_gradient* gradient, rizz_gradient_key key);
        bool (*remove_key)(rizz_gradient* gradient, int index);
        bool (*move_key)(rizz_gradient* gradient, int index, float t);
        void (*eval)(const rizz_gradient* gradient, float t, sx_color* outcolor);
        void (*edit)(const rizz_api_imgui* api, const char* label, rizz_gradient* gradient);
    } gradient;
    struct {
        void (*init)(rizz_graph* graph, float start, float end);
        bool (*add_key)(rizz_graph* graph, rizz_graph_key key);
        bool (*remove_key)(rizz_graph* graph, int index);
        bool (*move_key)(rizz_graph* graph, int index, float t, float value);
        float (*eval)(const rizz_graph* graph, float t);
        float (*eval_remap)(const rizz_graph* graph, float t, float t_min, float t_max, float v_min,
                            float v_max);
        void (*edit)(const rizz_api_imgui* api, const char* label, rizz_graph* graph,
                     sx_color color);
        void (*edit_multiple)(const rizz_api_imgui* api, rizz_graph** graphs, const char** names,
                              sx_color* colors, int* selected, int num_graphs);
    } graph;
} rizz_api_utility;