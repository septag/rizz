#pragma once

#include "rizz/rizz.h"
#include "rizz/util.h"

#include "sx/math.h"

typedef struct sx_alloc sx_alloc;

// config
// change this value to increase number of input parameters for anim controller
#ifndef RIZZ_SPRITE_ANIMCTRL_MAX_PARAMS
#    define RIZZ_SPRITE_ANIMCTRL_MAX_PARAMS 8
#endif

// if = 0, max_frames will be only bounded by heap memory (heap allocated)
#ifndef RIZZ_SPRITE_ANIMCLIP_MAX_FRAMES
#    define RIZZ_SPRITE_ANIMCLIP_MAX_FRAMES 0
#endif

#define RIZZ_SPRITE_ANIMCLIP_EVENT_END -1

// clang-format off
typedef struct { uint32_t id; } rizz_sprite;
typedef struct { uint32_t id; } rizz_sprite_animclip;
typedef struct { uint32_t id; } rizz_sprite_animctrl;
//clang-format on

typedef enum {
    RIZZ_SPRITE_FLIP_NONE = 0,
    RIZZ_SPRITE_FLIP_X = 0x1,
    RIZZ_SPRITE_FLIP_Y = 0x2,
} rizz_sprite_flip_;
typedef uint32_t rizz_sprite_flip;

typedef struct rizz_sprite_desc {
    const char* name;
    union {
        rizz_asset texture;
        rizz_asset atlas;
    };
    sx_vec2 size;
    sx_vec2 origin;    // origin relative to sprite's base rectangle's center.
                       // (0, 0) is on the center of sprite
                       // (-0.5f, -0.5f) is bottom-left of the sprite
                       // (0.5f, 0.5f) is the top-right of the sprite
    sx_color         color;
    rizz_sprite_flip flip;
    rizz_sprite_animclip clip;
    rizz_sprite_animctrl ctrl;
} rizz_sprite_desc;

typedef struct rizz_sprite_vertex {
    sx_vec2  pos;
    sx_vec2  uv;
    sx_color color;
} rizz_sprite_vertex;

typedef struct rizz_sprite_drawbatch {
    int        index_start;
    int        index_count;
    rizz_asset texture;
} rizz_sprite_drawbatch;

typedef struct rizz_sprite_drawsprite {
    int index;          // index to input (original) sprite array (see `sprite_drawdata_make_batch`)
    int start_vertex;
    int start_index;
    int num_verts;
    int num_indices;
} rizz_sprite_drawsprite;

typedef struct rizz_sprite_drawdata {
    rizz_sprite_drawsprite* sprites;
    rizz_sprite_drawbatch*  batches;
    rizz_sprite_vertex*     verts;
    uint16_t*               indices;
    int                     num_sprites;
    int                     num_batches;
    int                     num_verts;
    int                     num_indices;
} rizz_sprite_drawdata;

typedef struct rizz_atlas_info {
    int img_width;
    int img_height;
    int num_sprites;
} rizz_atlas_info;

typedef struct rizz_atlas {
    rizz_asset      texture;
    rizz_atlas_info info;
} rizz_atlas;

typedef struct rizz_atlas_load_params {
    int       first_mip;
    sg_filter min_filter;
    sg_filter mag_filter;
} rizz_atlas_load_params;

typedef struct rizz_sprite_animclip_frame_desc {
    const char* name;   // name reference in atlas
    rizz_event  event;
    bool        trigger_event;
} rizz_sprite_animclip_frame_desc;

typedef struct rizz_sprite_animclip_desc {
    rizz_asset                             atlas;
    int                                    num_frames;
    const sx_alloc*                        alloc;
    const rizz_sprite_animclip_frame_desc* frames;
    float                                  fps;
    float                                  length;
    rizz_sprite_flip                       flip;
    bool                                   trigger_end_event;
    rizz_event                             end_event;
} rizz_sprite_animclip_desc;

typedef struct rizz_sprite_animctrl_state_desc {
    const char*          name;
    rizz_sprite_animclip clip;
} rizz_sprite_animctrl_state_desc;

typedef enum rizz_sprite_animctrl_param_type {
    RIZZ_SPRITE_PARAMTYPE_INT,
    RIZZ_SPRITE_PARAMTYPE_FLOAT,
    RIZZ_SPRITE_PARAMTYPE_BOOL,
    RIZZ_SPRITE_PARAMTYPE_BOOL_AUTO    // flips itself then the condition is met
} rizz_sprite_animctrl_param_type;

typedef enum rizz_sprite_animctrl_compare_func {
    RIZZ_SPRITE_COMPAREFUNC_NONE,
    RIZZ_SPRITE_COMPAREFUNC_LESS,
    RIZZ_SPRITE_COMPAREFUNC_EQUAL,
    RIZZ_SPRITE_COMPAREFUNC_GREATER,
    RIZZ_SPRITE_COMPAREFUNC_NOT_EQUAL,
    RIZZ_SPRITE_COMPAREFUNC_GREATER_EQUAL,
    RIZZ_SPRITE_COMPAREFUNC_LESS_EQUAL,
    _RIZZ_SPRITE_COMPAREFUNC_COUNT
} rizz_sprite_animctrl_compare_func;

typedef struct rizz_sprite_animctrl_trigger {
    const char* param_name;     // NULL will set the trigger to "clip end" by default
    rizz_sprite_animctrl_compare_func func;
    union {
        int   i;
        float f;
        bool  b;
    } value;    
} rizz_sprite_animctrl_trigger;

typedef struct rizz_sprite_animctrl_transition_desc {
    const char* state;
    const char* target_state;
    rizz_sprite_animctrl_trigger trigger;
    bool                         trigger_event;
    rizz_event                   event;
} rizz_sprite_animctrl_transition_desc;

typedef struct rizz_sprite_animctrl_param_desc {
    const char*                 name;
    rizz_sprite_animctrl_param_type type;
} rizz_sprite_animctrl_param_desc;

typedef struct rizz_sprite_animctrl_desc {
    const sx_alloc*                             alloc;
    const rizz_sprite_animctrl_state_desc*      states;
    const rizz_sprite_animctrl_transition_desc* transitions;
    const char*                                 start_state;
    rizz_sprite_animctrl_param_desc             params[RIZZ_SPRITE_ANIMCTRL_MAX_PARAMS];
    int                                         num_states;
    int                                         num_transitions;
} rizz_sprite_animctrl_desc;

typedef struct rizz_api_sprite {
    rizz_sprite (*create)(const rizz_sprite_desc* desc);
    void (*destroy)(rizz_sprite spr);

    // if clip = {0}, it uses the source sprite clip handle
    rizz_sprite (*clone)(rizz_sprite spr, rizz_sprite_animclip clip);

    // atlas
    const rizz_atlas* (*atlas_get)(rizz_asset atlas_asset);

    // property accessors
    sx_vec2 (*size)(rizz_sprite spr);
    sx_vec2 (*origin)(rizz_sprite spr);
    sx_color (*color)(rizz_sprite spr);
    sx_rect (*bounds)(rizz_sprite spr);
    sx_rect (*draw_bounds)(rizz_sprite spr);
    rizz_sprite_flip (*flip)(rizz_sprite spr);
    const char* (*name)(rizz_sprite spr);

    void (*set_size)(rizz_sprite spr, const sx_vec2 size);
    void (*set_origin)(rizz_sprite spr, const sx_vec2 origin);
    void (*set_color)(rizz_sprite spr, const sx_color color);
    void (*set_flip)(rizz_sprite spr, rizz_sprite_flip flip);

    // draw-data: low-level API to construct geometry data for drawing
    rizz_sprite_drawdata* (*make_drawdata)(rizz_sprite spr, const sx_alloc* alloc);
    rizz_sprite_drawdata* (*make_drawdata_batch)(const rizz_sprite* sprs, int num_sprites,
                                                 const sx_alloc* alloc);
    void (*free_drawdata)(rizz_sprite_drawdata* data, const sx_alloc* alloc);

    // high-level draw calls, normal sprite drawing
    // internally calls `make_drawdata` and draws with internal shader and buffers
    void (*draw)(rizz_sprite spr, const sx_mat4* vp, const sx_mat3* mat, sx_color tint);
    void (*draw_batch)(const rizz_sprite* sprs, int num_sprites, const sx_mat4* vp,
                       const sx_mat3* mats, sx_color* tints);
    void (*draw_wireframe_batch)(const rizz_sprite* sprs, int num_sprites, const sx_mat4* vp,
                                 const sx_mat3* mats);                       
    void (*draw_wireframe)(rizz_sprite spr, const sx_mat4* vp, const sx_mat3* mat);
    bool (*resize_draw_limits)(int max_verts, int max_indices);

    // anim-clip
    rizz_sprite_animclip (*animclip_create)(const rizz_sprite_animclip_desc* desc);
    void (*animclip_destroy)(rizz_sprite_animclip clip);
    rizz_sprite_animclip (*animclip_clone)(rizz_sprite_animclip clip);
    
    void (*animclip_update)(rizz_sprite_animclip clip, float dt);
    void (*animclip_update_batch)(const rizz_sprite_animclip* clips, int num_clips, float dt);

    float (*animclip_fps)(rizz_sprite_animclip clip);
    float (*animclip_len)(rizz_sprite_animclip clip);
    rizz_sprite_flip (*animclip_flip)(rizz_sprite_animclip clip);
    rizz_event_queue* (*animclip_events)(rizz_sprite_animclip clip);
    void (*animclip_set_fps)(rizz_sprite_animclip clip, float fps);
    void (*animclip_set_len)(rizz_sprite_animclip clip, float len); 
    void (*animclip_set_flip)(rizz_sprite_animclip clip, rizz_sprite_flip flip);
    void (*animclip_restart)(rizz_sprite_animclip clip);
    
    // anim-controller
    rizz_sprite_animctrl (*animctrl_create)(const rizz_sprite_animctrl_desc* desc);
    void (*animctrl_destroy)(rizz_sprite_animctrl ctrl);
    void (*animctrl_update)(rizz_sprite_animctrl ctrl, float dt);
    void (*animctrl_update_batch)(const rizz_sprite_animctrl* ctrls, int num_ctrls, float dt);

    void (*animctrl_set_paramb)(rizz_sprite_animctrl ctrl, const char* name, bool b);
    void (*animctrl_set_parami)(rizz_sprite_animctrl ctrl, const char* name, int i);
    void (*animctrl_set_paramf)(rizz_sprite_animctrl ctrl, const char* name, float f);
    bool (*animctrl_param_valueb)(rizz_sprite_animctrl ctrl, const char* name);
    float (*animctrl_param_valuef)(rizz_sprite_animctrl ctrl, const char* name);
    int (*animctrl_param_valuei)(rizz_sprite_animctrl ctrl, const char* name);
    void (*animctrl_restart)(rizz_sprite_animctrl ctrl);
    rizz_event_queue* (*animctrl_events)(rizz_sprite_animctrl ctrl);

    // debugging
    void (*show_debugger)(bool* p_open);
} rizz_api_sprite;
