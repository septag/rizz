#pragma once

#include "sx/math-types.h"

typedef struct sx_alloc sx_alloc;
typedef struct sx_hashtbl sx_hashtbl;
typedef struct sx_handle_pool sx_handle_pool;
typedef struct rizz_api_imgui rizz_api_imgui;

typedef struct rizz_coll_shape_poly2d_t {
    int count;
    sx_vec2 verts[8];
    sx_vec2 norms[8];
} rizz_coll_shape_poly;

typedef struct rizz_coll_pair_t {
    uint64_t ent1;
    uint64_t ent2;
    uint32_t mask1;
    uint32_t mask2;
} rizz_coll_pair;

typedef struct rizz_coll_detect_result_t {
    rizz_coll_pair* pairs;
    int num_pairs;
} rizz_coll_detect_result;

typedef struct rizz_coll_query_result_t {
    uint64_t* ents;
    int num_ents;
} rizz_coll_query_result;

typedef struct rizz_coll_ray_t {
    sx_vec3 origin; // starting position
    sx_vec3 dir;    // normalized
    float len;      // length
} rizz_coll_ray;

typedef struct rizz_coll_rayhit_t {
    uint64_t ent;
    sx_vec3 normal;
    float t;        // 0..len
} rizz_coll_rayhit;

typedef struct rizz_coll_entity_data_t {
    sx_box               box;
    rizz_coll_shape_poly poly;
    sx_aabb              aabb;
    uint32_t             mask;
    bool                 is_static;
} rizz_coll_entity_data;

// debugging (activated with STRIKE_DEBUG_COLLISION=1)
typedef enum rizz_coll_debug_collision_mode_t {
    RIZZ_COLL_DEBUGCOLLISION_MODE_COLLISIONS = 0,
    RIZZ_COLL_DEBUGCOLLISION_MODE_COLLISION_HEATMAP,
    RIZZ_COLL_DEBUGCOLLISION_MODE_ENTITY_HEATMAP
} rizz_coll_debug_collision_mode;

typedef enum rizz_coll_debug_raycast_mode_t {
    RIZZ_COLL_DEBUGRAYCAST_MODE_RAYHITS = 0,
    RIZZ_COLL_DEBUGRAYCAST_MODE_RAYHIT_HEATMAP,
    RIZZ_COLL_DEBUGRAYCAST_MODE_RAYMARCH_HEATMAP
} rizz_coll_debug_raycast_mode;

typedef struct rizz_coll_context_t rizz_coll_context;

typedef struct rizz_api_coll {
    rizz_coll_context* (*create_context)(float map_size_x, float map_size_y, float grid_cell_size,
                                         const sx_alloc* alloc);
    void (*destroy_context)(rizz_coll_context* ctx);

    void (*add_boxes)(rizz_coll_context* ctx, const sx_box* boxes, const uint64_t* ents,
                      const uint32_t* masks, const sx_tx3d* transforms, int count);
    void (*add_static_polys)(rizz_coll_context* ctx, const rizz_coll_shape_poly* polys,
                             const uint64_t* ents, const uint32_t* masks, int count);
    void (*remove)(rizz_coll_context* ctx, const uint64_t* ents, int count);
    void (*remove_all)(rizz_coll_context* ctx);

    void (*update_transforms)(rizz_coll_context* ctx, const uint64_t* ents, int count,
                              const sx_tx3d* new_transforms);

    // returns sx_array type for rizz_coll_pair, must be freed with sx_array_free(alloc)
    rizz_coll_pair* (*detect)(rizz_coll_context* ctx, const sx_alloc* alloc);
    uint64_t* (*query_sphere)(rizz_coll_context* ctx, sx_vec3 center, float radius, uint32_t mask,
                              const sx_alloc* alloc);
    uint64_t* (*query_poly)(rizz_coll_context* ctx, const rizz_coll_shape_poly* poly, uint32_t mask,
                            const sx_alloc* alloc);
    rizz_coll_rayhit* (*query_ray)(rizz_coll_context* ctx, rizz_coll_ray ray, uint32_t mask,
                                   const sx_alloc* alloc);

    // only available in STRIKE_COLLISION_DEBUG=1
    void (*debug_collisions)(rizz_coll_context* ctx, float opacity,
                             rizz_coll_debug_collision_mode mode, float heatmap_limit);
    void (*debug_raycast)(rizz_coll_context* ctx, float opacity, rizz_coll_debug_raycast_mode mode,
                          float heatmap_limit);

    int (*num_cells)(rizz_coll_context* ctx, int* num_cells_x, int* num_cells_y);
    sx_rect (*cell_rect)(rizz_coll_context* ctx, int cell_idx);

    bool (*get_entity_data)(rizz_coll_context* ctx, uint64_t ent, rizz_coll_entity_data* outdata);

} rizz_api_coll;

SX_INLINE rizz_coll_ray rizz_coll_ray_set(sx_vec3 origin, sx_vec3 dir, float len)
{
#ifndef __cplusplus
    return (rizz_coll_ray) {
        .origin = origin,
        .dir = dir,
        .len = len
    };
#else
    return { origin, dir, len };
#endif
}
