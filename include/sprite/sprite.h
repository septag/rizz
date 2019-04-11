#pragma once

#include "rizz/types.h"
#include "sx/math.h"

typedef struct sx_alloc sx_alloc;

typedef struct {
    uint32_t id;
} rizz_sprite;

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

typedef struct rizz_sprite_drawdata {
    rizz_sprite_drawbatch* batches;
    rizz_sprite_vertex*    verts;
    uint16_t*              indices;
    int                    num_batches;
    int                    num_verts;
    int                    num_indices;
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

typedef struct rizz_api_sprite {
    rizz_sprite (*sprite_create)(const rizz_sprite_desc* desc);
    void (*sprite_destroy)(rizz_sprite spr);

    // property accessors
    sx_vec2 (*sprite_size)(rizz_sprite spr);
    sx_vec2 (*sprite_origin)(rizz_sprite spr);
    sx_color (*sprite_color)(rizz_sprite spr);
    sx_rect (*sprite_bounds)(rizz_sprite spr);
    rizz_sprite_flip (*sprite_flip)(rizz_sprite spr);
    const char* (*sprite_name)(rizz_sprite spr);

    void (*sprite_set_size)(rizz_sprite spr, const sx_vec2 size);
    void (*sprite_set_origin)(rizz_sprite spr, const sx_vec2 origin);
    void (*sprite_set_color)(rizz_sprite spr, const sx_color color);
    void (*sprite_set_flip)(rizz_sprite spr, rizz_sprite_flip flip);

    // draw-data
    rizz_sprite_drawdata* (*sprite_drawdata_make)(rizz_sprite spr, const sx_alloc* alloc);
    rizz_sprite_drawdata* (*sprite_drawdata_make_batch)(const rizz_sprite* sprs, int num_sprites,
                                                        const sx_alloc* alloc);
    void (*sprite_drawdata_free)(rizz_sprite_drawdata* data, const sx_alloc* alloc);
} rizz_api_sprite;
