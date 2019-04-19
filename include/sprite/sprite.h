#pragma once

#include "rizz/_sg-types.h"
#include "sx/math.h"

typedef struct sx_alloc sx_alloc;

#ifndef RIZZ_SPRITE_ANIM_MAX_PARAMS
#   define RIZZ_SPRITE_ANIM_MAX_PARAMS 16
#endif

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
    int index;    // index to input sprite array (see `sprite_drawdata_make_batch`)
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

typedef struct rizz_api_sprite {
    rizz_sprite (*create)(const rizz_sprite_desc* desc);
    void (*destroy)(rizz_sprite spr);

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
    void (*draw)(rizz_sprite spr, const sx_mat4* vp, const sx_mat3* mat, sx_color tint,
                 const sx_alloc* alloc);
    void (*draw_batch)(const rizz_sprite* sprs, int num_sprites, const sx_mat4* vp,
                       const sx_mat3* mats, const sx_color* tints, const sx_alloc* alloc);
    void (*draw_wireframe_batch)(const rizz_sprite* sprs, int num_sprites, const sx_mat4* vp,
                                 const sx_mat3* mats, const sx_color* tints, const sx_alloc* alloc);                       
    void (*draw_wireframe)(rizz_sprite spr, const sx_mat4* vp, const sx_mat3* mat, sx_color tint, 
                           const sx_alloc* alloc);
    bool (*resize_draw_limits)(int max_verts, int max_indices);

    // debugging
    void (*debug_enable)(rizz_sprite spr);
    void (*debug_disable)(rizz_sprite spr);
    void (*debug_show_window)(bool* p_open);
    void (*debug_render)();
} rizz_api_sprite;
