#include "rizz/rizz.h"
#include "rizz/collision.h"

#include "sx/allocator.h"
#include "sx/array.h"
#include "sx/hash.h"
#include "sx/handle.h"
#include "sx/math-vec.h"
#include "sx/string.h"

#ifndef STRIKE_DEBUG_COLLISION
#   define STRIKE_DEBUG_COLLISION 1
#endif

#if STRIKE_DEBUG_COLLISION
#   include "rizz/imgui.h"
#   include "rizz/imgui-extra.h"
#endif

#define SORT_NAME coll__sort_u32
#define SORT_TYPE uint32_t
#define SORT_CMP(x, y) ((x) < (y) ? -1 : 1)
SX_PRAGMA_DIAGNOSTIC_PUSH()
SX_PRAGMA_DIAGNOSTIC_IGNORED_MSVC(4267)
SX_PRAGMA_DIAGNOSTIC_IGNORED_MSVC(4244)
SX_PRAGMA_DIAGNOSTIC_IGNORED_MSVC(4146)
SX_PRAGMA_DIAGNOSTIC_IGNORED_CLANG_GCC("-Wunused-function")
SX_PRAGMA_DIAGNOSTIC_IGNORED_CLANG("-Wshorten-64-to-32")
#include "sort/sort.h"

#undef SORT_NAME 
#undef SORT_TYPE 
#undef SORT_CMP
#define SORT_NAME coll__sort_rayhit
#define SORT_TYPE rizz_coll_rayhit
#define SORT_CMP(x, y) ((x).t - (y).t)
#include "sort/sort.h"
SX_PRAGMA_DIAGNOSTIC_POP()

#define CUTE_C2_IMPLEMENTATION
SX_PRAGMA_DIAGNOSTIC_PUSH()
SX_PRAGMA_DIAGNOSTIC_IGNORED_CLANG("-Wswitch")
SX_PRAGMA_DIAGNOSTIC_IGNORED_CLANG("-Wunused-function")
SX_PRAGMA_DIAGNOSTIC_IGNORED_GCC("-Wmaybe-uninitialized")
#include "cute_headers/cute_c2.h"
SX_PRAGMA_DIAGNOSTIC_POP()
#undef CUTE_C2_IMPLEMENTATION

typedef struct coll_spatial_grid_cell_t {
    sx_handle_t* ents;  // sx_array: handle to coll_context arrays
    sx_ivec2 pos_grid;  // integer position in the grid
    sx_vec2 center;     // the actual center on x-y plane

#ifdef STRIKE_DEBUG_COLLISION
    int num_raymarches;
    int num_rayhits;
    int num_collisions;
#endif
} coll_spatial_grid_cell;

// put entity/mask together for data coherency
typedef struct coll_entity_mask_pair_t {
    uint64_t entity;
    uint32_t mask;
} coll_entity_mask_pair;

typedef struct rizz_coll_context_t {
    const sx_alloc* alloc;
    sx_hashtbl* ent_tbl;                    // key = entity(uint64_t) -> handle to arrays
    sx_handle_pool* handles;                // handle pool for arrays below
    coll_entity_mask_pair* ent_mask_pairs;  // sx_array
    sx_aabb* aabbs;                         // sx_array
    rizz_coll_shape_poly* polys;            // sx_array
    sx_box* boxes;                          // sx_array: .e.x == .e.y == .e.z == 0 if static/poly only
    sx_aabb* transformed_aabbs;             // sx_array
    sx_box* transformed_boxes;              // sx_array
#if STRIKE_DEBUG_COLLISION
    int64_t* collision_frames;              // sx_array: (debug only) frame number for each entity that is collided
    int64_t* rayhit_frames;                 // sx_array: (debug only) frame number for each entity that is rayhit
    int64_t* raymarch_frames;               // sx_array: (debug only) frame number for each entity ray-march
    rizz_coll_ray* rays;                    // sx_array: (debug only) total rays that are casted 
#endif
    float map_size_x;                       // real logical dim
    float map_size_y;                       // real logical dim
    float grid_cell_size;                   // square cell (each dim = size)
    int num_cells_x;
    int num_cells_y;
    int num_cells;
    coll_spatial_grid_cell* cells;
    sx_handle_t* updated_ent_handles;
} rizz_coll_context;

RIZZ_STATE static rizz_api_plugin* the_plugin;
RIZZ_STATE static rizz_api_core* the_core; 
RIZZ_STATE static rizz_api_imgui* the_imgui;
RIZZ_STATE static rizz_api_imgui_extra* the_imguix;
RIZZ_STATE static rizz_api_app* the_app;
RIZZ_STATE static rizz_api_gfx* the_gfx;

static rizz_coll_context* coll_create_context(float map_size_x, float map_size_y,
                                              float grid_cell_size, const sx_alloc* alloc)
{
    sx_assert(alloc);

    rizz_coll_context* ctx = sx_malloc(alloc, sizeof(rizz_coll_context));
    if (!ctx) {
        sx_out_of_memory();
        return NULL;
    }
    sx_memset(ctx, 0x0, sizeof(rizz_coll_context));

    ctx->alloc = alloc;
    ctx->ent_tbl = sx_hashtbl_create(alloc, 1024);
    if (!ctx->ent_tbl) {
        sx_out_of_memory();
        return NULL;
    }

    ctx->handles = sx_handle_create_pool(alloc, 1024);
    if (!ctx->handles) {
        sx_out_of_memory();
        return NULL;
    }

    sx_assert(sx_mod(map_size_x, grid_cell_size) == 0);
    sx_assert(sx_mod(map_size_y, grid_cell_size) == 0);
    ctx->grid_cell_size = grid_cell_size;
    ctx->map_size_x = map_size_x;
    ctx->map_size_y = map_size_y; 
    ctx->num_cells_x = (int)(map_size_x / grid_cell_size);
    ctx->num_cells_y = (int)(map_size_y / grid_cell_size);
    ctx->num_cells = ctx->num_cells_x * ctx->num_cells_y;
    ctx->cells = sx_malloc(alloc, ctx->num_cells * sizeof(coll_spatial_grid_cell));
    if (!ctx->cells) {
        sx_out_of_memory();
        return NULL;
    }
    sx_memset(ctx->cells, 0x0, sizeof(coll_spatial_grid_cell)*ctx->num_cells);

    float ymin = -map_size_y*0.5f;
    for (int y = 0; y < ctx->num_cells_y; y++) {
        float xmin = -map_size_x*0.5f;
        for (int x = 0; x < ctx->num_cells_x; x++) {
            int index = y*ctx->num_cells_x + x;
            coll_spatial_grid_cell* cell = &ctx->cells[index];

            cell->pos_grid = sx_ivec2i(x, y);
            cell->center = sx_vec2f(xmin + ctx->grid_cell_size*0.5f, ymin + ctx->grid_cell_size*0.5f);
            xmin += ctx->grid_cell_size;
        }
        ymin += ctx->grid_cell_size;
    }
    
    return ctx;
}

static void coll_destroy_context(rizz_coll_context* ctx)
{
    if (!ctx) {
        return;
    }

    const sx_alloc* alloc = ctx->alloc;
    if (!alloc) {
        return;
    }

    for (int i = 0; i < ctx->num_cells; i++) {
        coll_spatial_grid_cell* cell = &ctx->cells[i];
        sx_array_free(alloc, cell->ents);
    }
    sx_free(alloc, ctx->cells);

    sx_array_free(alloc, ctx->ent_mask_pairs);
    sx_array_free(alloc, ctx->aabbs);
    sx_array_free(alloc, ctx->boxes);
    sx_array_free(alloc, ctx->polys);
    sx_array_free(alloc, ctx->transformed_boxes);
    sx_array_free(alloc, ctx->transformed_aabbs);
    #if STRIKE_DEBUG_COLLISION
        sx_array_free(alloc, ctx->collision_frames);
        sx_array_free(alloc, ctx->rayhit_frames);
        sx_array_free(alloc, ctx->raymarch_frames);
        sx_array_free(alloc, ctx->rays);
    #endif // STRIKE_DEBUG_COLLISION

    sx_array_free(alloc, ctx->updated_ent_handles);
    sx_hashtbl_destroy(ctx->ent_tbl, alloc);
    sx_handle_destroy_pool(ctx->handles, alloc);

    sx_free(alloc, ctx);
}

static inline void coll__calc_poly_from_box(const sx_box* box, c2Poly* _c2p, c2x* _c2x)
{
    const float _aa = 0.70710678f;

    _c2p->count = 4;
    _c2p->verts[0] = (c2v) { box->e.x, box->e.y };
    _c2p->norms[0] = (c2v) { _aa, _aa };
    _c2p->verts[1] = (c2v) { -box->e.x, box->e.y };
    _c2p->norms[1] = (c2v) { -_aa, _aa };
    _c2p->verts[2] = (c2v) { -box->e.x, -box->e.y };
    _c2p->norms[2] = (c2v) { -_aa, -_aa };
    _c2p->verts[3] = (c2v) { box->e.x, -box->e.y };
    _c2p->norms[3] = (c2v) { _aa, -_aa };
    
    float theta = sx_atan2(box->tx.rot.m21, box->tx.rot.m11);
    _c2x->r.c = sx_cos(theta);
    _c2x->r.s = sx_sin(theta);
    _c2x->p.x = box->tx.pos.x;
    _c2x->p.y = box->tx.pos.y;
}

static inline sx_ivec2 coll__hash_point(rizz_coll_context* ctx, sx_vec2 p)
{
    float cell_size = ctx->grid_cell_size;
    float hx = p.x/cell_size + (float)ctx->num_cells_x*0.5f;
    float hy = p.y/cell_size + (float)ctx->num_cells_y*0.5f;

    int hxi = sx_clamp((int)hx, 0, ctx->num_cells_x - 1);
    int hyi = sx_clamp((int)hy, 0, ctx->num_cells_y - 1);
    return sx_ivec2i(hxi, hyi);
}

static inline int coll__cell_id(int x, int y, int const num_cells_x)
{
    return y*num_cells_x + x;
}

static void coll_add_boxes(rizz_coll_context* ctx, const sx_box* boxes, const uint64_t* ents,
                           const uint32_t* masks, const sx_tx3d* transforms, int count)
{
    const sx_alloc* alloc = ctx->alloc;
    int const num_cells_x = ctx->num_cells_x;

    for (int i = 0; i < count; i++) {
        int handles_count = ctx->handles->count;
        sx_handle_t handle = sx_handle_new_and_grow(ctx->handles, alloc);
        sx_assert_always(handle);

        int index = sx_handle_index(handle);
        rizz_coll_shape_poly poly = { 0 };
        sx_aabb aabb = sx_aabb_from_box(&boxes[i]);
        coll_entity_mask_pair em_pair = {
            .entity = ents[i],
            .mask = masks[i]
        };
        sx_assert((boxes[i].e.x + boxes[i].e.y + boxes[i].e.z) != 0);

        sx_mat4 transform_mat = sx_tx3d_mat4(&transforms[i]);
        sx_aabb transformed_aabb = sx_aabb_transform(&aabb, &transform_mat);
        sx_box transformed_box = sx_box_set(sx_tx3d_mul(&transforms[i], &boxes[i].tx), boxes[i].e);

        if (index >= handles_count) {
            sx_array_push(alloc, ctx->ent_mask_pairs, em_pair);
            sx_array_push(alloc, ctx->polys, poly);
            sx_array_push(alloc, ctx->boxes, boxes[i]);
            sx_array_push(alloc, ctx->aabbs, aabb);
            #if STRIKE_DEBUG_COLLISION
                sx_array_push(alloc, ctx->collision_frames, 0);
                sx_array_push(alloc, ctx->rayhit_frames, 0);
                sx_array_push(alloc, ctx->raymarch_frames, 0);
            #endif
            sx_array_push(alloc, ctx->transformed_aabbs, transformed_aabb);
            sx_array_push(alloc, ctx->transformed_boxes, transformed_box);
        } else {
            ctx->ent_mask_pairs[index] = em_pair;
            ctx->polys[index] = poly;
            ctx->boxes[index] = boxes[i];
            ctx->aabbs[index] = aabb;
            #if STRIKE_DEBUG_COLLISION
                ctx->collision_frames[index] = 0;
                ctx->rayhit_frames[index] = 0;
                ctx->raymarch_frames[index] = 0;
            #endif
            ctx->transformed_aabbs[index] = transformed_aabb;
            ctx->transformed_boxes[index] = transformed_box;
        }

        { // push AABB to the spatial grid
            sx_ivec2 hmin = coll__hash_point(ctx, sx_vec2f(transformed_aabb.xmin, transformed_aabb.ymin));
            sx_ivec2 hmax = coll__hash_point(ctx, sx_vec2f(transformed_aabb.xmax, transformed_aabb.ymax));
            for (int y = hmin.y; y <= hmax.y; y++) {
                for (int x = hmin.x; x <= hmax.x; x++) {
                    sx_array_push(alloc, ctx->cells[coll__cell_id(x, y, num_cells_x)].ents, handle);
                }
            }
        }
        
        sx_hashtbl_add_and_grow(ctx->ent_tbl, sx_hash_u64_to_u32(ents[i]), (int)handle, ctx->alloc);
    }
}

static void coll_add_static_polys(rizz_coll_context* ctx, const rizz_coll_shape_poly* polys,
                                  const uint64_t* ents, const uint32_t* masks, int count)
{
    const sx_alloc* alloc = ctx->alloc;
    int const num_cells_x = ctx->num_cells_x;
    sx_box empty_box = sx_box_set(sx_tx3d_ident(), SX_VEC3_ZERO);

    for (int i = 0; i < count; i++) {
        int handles_count = ctx->handles->count;
        sx_handle_t handle = sx_handle_new_and_grow(ctx->handles, alloc);
        sx_assert_always(handle);
        int index = sx_handle_index(handle);

        const rizz_coll_shape_poly* poly = &polys[i];
        sx_aabb aabb = sx_aabb_empty();
        for (int ii = 0; ii < poly->count; ii++) {
            sx_aabb_add_point(&aabb, sx_vec3fv(poly->verts[ii].f));
        }
        coll_entity_mask_pair em_pair = {
            .entity = ents[i],
            .mask = masks[i]
        };

        if (index >= handles_count) {
            sx_array_push(alloc, ctx->ent_mask_pairs, em_pair);
            sx_array_push(alloc, ctx->polys, *poly);
            sx_array_push(alloc, ctx->boxes, empty_box);
            sx_array_push(alloc, ctx->aabbs, aabb);
            #if STRIKE_DEBUG_COLLISION
                sx_array_push(alloc, ctx->collision_frames, 0);
                sx_array_push(alloc, ctx->rayhit_frames, 0);
                sx_array_push(alloc, ctx->raymarch_frames, 0);
            #endif
            sx_array_push(alloc, ctx->transformed_aabbs, aabb);
            sx_array_push(alloc, ctx->transformed_boxes, empty_box);
        } else {
            ctx->ent_mask_pairs[index] = em_pair;
            ctx->polys[index] = *poly;
            ctx->boxes[index] = empty_box;
            ctx->aabbs[index] = aabb;
            #if STRIKE_DEBUG_COLLISION
                ctx->collision_frames[index] = 0;
                ctx->rayhit_frames[index] = 0;
                ctx->raymarch_frames[index] = 0;
            #endif
            ctx->transformed_aabbs[index] = aabb;
            ctx->transformed_boxes[index] = empty_box;
        }

        { // push AABB to the spatial grid
            sx_ivec2 hmin = coll__hash_point(ctx, sx_vec2f(aabb.xmin, aabb.ymin));
            sx_ivec2 hmax = coll__hash_point(ctx, sx_vec2f(aabb.xmax, aabb.ymax));
            for (int y = hmin.y; y <= hmax.y; y++) {
                for (int x = hmin.x; x <= hmax.x; x++) {
                    sx_array_push(alloc, ctx->cells[coll__cell_id(x, y, num_cells_x)].ents, handle);
                }
            }
        }
        
        sx_hashtbl_add_and_grow(ctx->ent_tbl, sx_hash_u64_to_u32(ents[i]), (int)handle, ctx->alloc);
    }
}


static void coll_update_transforms(rizz_coll_context* ctx, const uint64_t* ents, int count,
                                   const sx_tx3d* new_transforms)
{
    int const num_cells_x = ctx->num_cells_x;
    for (int i = 0; i < count; i++) {
        sx_handle_t handle = (sx_handle_t)sx_hashtbl_find_get(ctx->ent_tbl, sx_hash_u64_to_u32(ents[i]), 0);
        if (!handle) {
            rizz_log_warn("Entity handle (%I64d) could not be found in collision database", ents[i]);
            continue;
        }

        int index = sx_handle_index(handle);

        sx_aabb aabb = ctx->aabbs[index];
        sx_aabb prev_aabb = ctx->transformed_aabbs[index];
        sx_ivec2 prev_hmin = coll__hash_point(ctx, sx_vec2fv(prev_aabb.vmin));
        sx_ivec2 prev_hmax = coll__hash_point(ctx, sx_vec2fv(prev_aabb.vmax));
        sx_irect prev_area = sx_irectv(prev_hmin, prev_hmax);

        // transform and pop/push from spatial grid
        sx_mat4 transform_mat = sx_tx3d_mat4(&new_transforms[i]);
        aabb = sx_aabb_transform(&aabb, &transform_mat);
        sx_ivec2 hmin = coll__hash_point(ctx, sx_vec2fv(aabb.vmin));
        sx_ivec2 hmax = coll__hash_point(ctx, sx_vec2fv(aabb.vmax));
        sx_irect area = sx_irectv(hmin, hmax);

        ctx->transformed_aabbs[index] = aabb;
        ctx->transformed_boxes[index] =
            sx_box_set(sx_tx3d_mul(&new_transforms[i], &ctx->boxes[index].tx), ctx->boxes[index].e);

        // try to remove the entity from the cells that it does not reside anymore after transform
        for (int y = prev_hmin.y; y <= prev_hmax.y; y++) {
            for (int x = prev_hmin.x; x <= prev_hmax.x; x++) {
                if (!sx_irect_test_point(area, sx_ivec2i(x, y))) {
                    // cell doesn't exist in the new cells area, remove from this old cell
                    coll_spatial_grid_cell* cell = &ctx->cells[coll__cell_id(x, y, num_cells_x)];
                    for (int e = 0, ec = sx_array_count(cell->ents); e < ec; e++) {
                        if (cell->ents[e] == handle) {
                            sx_array_pop(cell->ents, e);
                            break;
                        }
                    }
                } 
            } // foreach(x)
        } // foreach(y)

        // add the entity to the new cells
        for (int y = hmin.y; y <= hmax.y; y++) {
            for (int x = hmin.x; x <= hmax.x; x++) {
                if (!sx_irect_test_point(prev_area, sx_ivec2i(x, y))) {
                    // new cell does not collapse with the old one, so this is a new cell
                    // add it to the new cell
                    coll_spatial_grid_cell* cell = &ctx->cells[coll__cell_id(x, y, num_cells_x)];
                    sx_array_push(ctx->alloc, cell->ents, handle);
                } 
            } // foreach(x)
        } // foreach(y)

        sx_array_push(ctx->alloc, ctx->updated_ent_handles, handle);
    }
}

#if STRIKE_DEBUG_COLLISION
static void coll__mark_collision(rizz_coll_context* ctx, const sx_aabb* aabb)
{
    int const num_cells_x = ctx->num_cells_x;
    sx_ivec2 hmin = coll__hash_point(ctx, sx_vec2fv(aabb->vmin));
    sx_ivec2 hmax = coll__hash_point(ctx, sx_vec2fv(aabb->vmax));

    for (int y = hmin.y; y <= hmax.y; y++) {
        for (int x = hmin.x; x <= hmax.x; x++) {
            coll_spatial_grid_cell* cell = &ctx->cells[coll__cell_id(x, y, num_cells_x)];
            ++cell->num_collisions;
        }
    }
}

static void coll__mark_rayhit(rizz_coll_context* ctx, const sx_aabb* aabb)
{
    int const num_cells_x = ctx->num_cells_x;
    sx_ivec2 hmin = coll__hash_point(ctx, sx_vec2fv(aabb->vmin));
    sx_ivec2 hmax = coll__hash_point(ctx, sx_vec2fv(aabb->vmax));

    for (int y = hmin.y; y <= hmax.y; y++) {
        for (int x = hmin.x; x <= hmax.x; x++) {
            coll_spatial_grid_cell* cell = &ctx->cells[coll__cell_id(x, y, num_cells_x)];
            ++cell->num_rayhits;
        }
    }
}
#endif // STRIKE_DEBUG_COLLISION

static bool coll__check_existing_pair(const rizz_coll_pair* pairs, int num_pairs, rizz_coll_pair pair)
{
    for (int i = 0; i < num_pairs; i++) {
        if ((pairs[i].ent1 == pair.ent1 && pairs[i].ent2 == pair.ent2) ||
            (pairs[i].ent2 == pair.ent1 && pairs[i].ent1 == pair.ent2))
        {
            return true;
        }
    }
    return false;
}

static rizz_coll_pair* coll_detect(rizz_coll_context* ctx, const sx_alloc* alloc)
{
    int const num_cells_x = ctx->num_cells_x;
    rizz_coll_pair* pairs = NULL;

    const sx_alloc* tmp_alloc = the_core->tmp_alloc_push();
    sx_scope(the_core->tmp_alloc_pop()) {

        sx_array_reserve(alloc, pairs, 50);

        #if STRIKE_DEBUG_COLLISION
            int64_t frame_id = the_core->frame_index(); // used for debugging only
            for (int i = 0; i < ctx->num_cells; i++) {
                ctx->cells[i].num_collisions = 0;
            }
        #endif // STRIKE_DEBUG_COLLISION
    
        for (int i = 0, ic = sx_array_count(ctx->updated_ent_handles); i < ic; i++) {
            sx_handle_t handle = ctx->updated_ent_handles[i];
            int index = sx_handle_index(handle);

            sx_handle_t* candidates = NULL;

            // collect all candidates for collision
            sx_aabb aabb = ctx->transformed_aabbs[index];
            sx_ivec2 hmin = coll__hash_point(ctx, sx_vec2fv(aabb.vmin));
            sx_ivec2 hmax = coll__hash_point(ctx, sx_vec2fv(aabb.vmax));

            for (int y = hmin.y; y <= hmax.y; y++) {
                for (int x = hmin.x; x <= hmax.x; x++) {
                    coll_spatial_grid_cell cell = ctx->cells[coll__cell_id(x, y, num_cells_x)];
                    int num_cell_ents = sx_array_count(cell.ents);
                    sx_memcpy(sx_array_add(tmp_alloc, candidates, num_cell_ents), cell.ents,
                              num_cell_ents * sizeof(sx_handle_t));
                }
            }

            // remove duplicates
            int num_candidates = sx_array_count(candidates);
            //coll__sort_u32_quick_sort(candidates, num_candidates);
            coll__sort_u32_tim_sort(candidates, num_candidates);
            for (int e = 0; e < (num_candidates - 1); e++) {
                if (candidates[e] == candidates[e+1]) {
                    sx_array_pop(candidates, e);
                    --e;
                    --num_candidates;
                }
            }

            // perform narrow phase
            sx_box box = ctx->transformed_boxes[index];
            c2Poly poly;
            c2x polytx;
            coll_entity_mask_pair ent_mask = ctx->ent_mask_pairs[index];
            coll__calc_poly_from_box(&box, &poly, &polytx);
        
            for (int e = 0; e < num_candidates; e++) {
                int test_index = sx_handle_index(candidates[e]);
                sx_aabb test_aabb = ctx->transformed_aabbs[test_index];
                coll_entity_mask_pair test_ent_mask = ctx->ent_mask_pairs[test_index];

                if ((ent_mask.mask & test_ent_mask.mask) == 0 || !sx_aabb_test(&aabb, &test_aabb)) {
                    continue;
                }

                sx_box test_box = ctx->transformed_boxes[test_index];
                if ((test_box.e.x + test_box.e.y + test_box.e.z) > 0.00001f) {
                    c2Poly poly2;
                    c2x polytx2;
                    coll__calc_poly_from_box(&test_box, &poly2, &polytx2);
                    if (!c2PolytoPoly(&poly, &polytx, &poly2, &polytx2)) {
                        continue;
                    }
                } else {
                    // static poly
                    if (!c2PolytoPoly(&poly, &polytx, (const c2Poly*)&ctx->polys[test_index], NULL)) {
                        continue;
                    }
                }

                if (ent_mask.entity != test_ent_mask.entity) {
                    rizz_coll_pair pair = { 
                        .ent1 = ent_mask.entity,
                        .ent2 = test_ent_mask.entity,
                        .mask1 = ent_mask.mask,
                        .mask2 = test_ent_mask.mask
                    };

                    #if STRIKE_DEBUG_COLLISION
                        ctx->collision_frames[index] = frame_id;
                        ctx->collision_frames[test_index] = frame_id;
                        coll__mark_collision(ctx, &aabb);
                        coll__mark_collision(ctx, &test_aabb);
                    #endif

                    if (!coll__check_existing_pair(pairs, sx_array_count(pairs), pair)) {
                        sx_array_push(alloc, pairs, pair);
                    }
                }
            } // foreach (candidate)
                
        } // foreach (upadted_entity_handles)

        sx_array_clear(ctx->updated_ent_handles);
    } // scope

    return pairs;
}

static void coll_remove(rizz_coll_context* ctx, const uint64_t* ents, int count)
{
    int const num_cells_x = ctx->num_cells_x;

    for (int i = 0; i < count; i++) {
        uint64_t ent = ents[i];
        int tbl_idx = sx_hashtbl_find(ctx->ent_tbl, sx_hash_u64_to_u32(ent));
        if (tbl_idx == -1) {
            rizz_log_warn("Entity handle (%I64d) could not be found in collision database", ent);
            return;
        }

        sx_handle_t handle = sx_hashtbl_get(ctx->ent_tbl, tbl_idx);
        int index = sx_handle_index(handle);

        sx_aabb aabb = ctx->transformed_aabbs[index];
        sx_ivec2 hmin = coll__hash_point(ctx, sx_vec2fv(aabb.vmin));
        sx_ivec2 hmax = coll__hash_point(ctx, sx_vec2fv(aabb.vmax));

        for (int y = hmin.y; y <= hmax.y; y++) {
            for (int x = hmin.x; x <= hmax.x; x++) {
                coll_spatial_grid_cell* cell = &ctx->cells[coll__cell_id(x, y, num_cells_x)];
                for (int e = 0, ec = sx_array_count(cell->ents); e < ec; e++) {
                    if (cell->ents[e] == ent) {
                        sx_array_pop(cell->ents, e);
                        break;
                    }
                } // foreach (cell->ents)
            } // foreach(x)
        } // foreach(y)

        sx_handle_del(ctx->handles, handle);
        sx_hashtbl_remove(ctx->ent_tbl, tbl_idx);
    }
}

static void coll_remove_all(rizz_coll_context* ctx)
{
    for (int i = 0, ic = ctx->num_cells; i < ic; i++) {
        coll_spatial_grid_cell* cell = &ctx->cells[i];
        sx_array_clear(cell->ents);
        #if STRIKE_DEBUG_COLLISION
            cell->num_collisions = 0;
            cell->num_raymarches = 0;
            cell->num_rayhits = 0;
        #endif
    }
    sx_hashtbl_clear(ctx->ent_tbl);
    sx_handle_reset_pool(ctx->handles);
}

static uint64_t* coll_query_sphere(rizz_coll_context* ctx, sx_vec3 center, float radius, uint32_t mask, const sx_alloc* alloc)
{
    int const num_cells_x = ctx->num_cells_x;
    sx_aabb aabb = sx_aabbv(sx_vec3_sub(center, sx_vec3splat(radius)), sx_vec3_add(center, sx_vec3splat(radius)));
    sx_ivec2 hmin = coll__hash_point(ctx, sx_vec2fv(aabb.vmin));
    sx_ivec2 hmax = coll__hash_point(ctx, sx_vec2fv(aabb.vmax));
    uint64_t* ents = NULL;

    const sx_alloc* tmp_alloc = the_core->tmp_alloc_push();
    sx_scope(the_core->tmp_alloc_pop()) {

        // broad-phase
        sx_handle_t* candidates = NULL;
        for (int y = hmin.y; y <= hmax.y; y++) {
            for (int x = hmin.x; x <= hmax.x; x++) {
                coll_spatial_grid_cell cell = ctx->cells[coll__cell_id(x, y, num_cells_x)];
                int num_cell_ents = sx_array_count(cell.ents);
                sx_memcpy(sx_array_add(tmp_alloc, candidates, num_cell_ents), cell.ents,
                            num_cell_ents * sizeof(sx_handle_t));
            }
        }

        // remove duplicates
        int num_candidates = sx_array_count(candidates);
        coll__sort_u32_tim_sort(candidates, num_candidates);
        for (int e = 0; e < (num_candidates - 1); e++) {
            if (candidates[e] == candidates[e+1]) {
                sx_array_pop(candidates, e);
                --e;
                --num_candidates;
            }
        }

        c2Circle circle;
        circle.p.x = center.x;
        circle.p.y = center.y;
        circle.r = radius;

        for (int e = 0; e < num_candidates; e++) {
            int test_index = sx_handle_index(candidates[e]);
            sx_aabb test_aabb = ctx->transformed_aabbs[test_index];
            coll_entity_mask_pair test_entmask = ctx->ent_mask_pairs[test_index];

            if ((mask & test_entmask.mask) == 0 || !sx_aabb_test(&aabb, &test_aabb)) {
                continue;
            }

            sx_box test_box = ctx->transformed_boxes[test_index];
            if ((test_box.e.x + test_box.e.y + test_box.e.z) > 0.00001f) {
                c2Poly poly2;
                c2x polytx2;
                coll__calc_poly_from_box(&test_box, &poly2, &polytx2);
                if (!c2CircletoPoly(circle, &poly2, &polytx2)) {
                    continue;
                }
            } else {
                // static poly
                if (!c2CircletoPoly(circle, (const c2Poly*)&ctx->polys[test_index], NULL)) {
                    continue;
                }
            }

            sx_array_push(alloc, ents, test_entmask.entity);
        } // foreach (candidate)
    } // scope

    return ents;
}

static uint64_t* coll_query_poly(rizz_coll_context* ctx, const rizz_coll_shape_poly* poly,
                                 uint32_t mask, const sx_alloc* alloc)
{
    int const num_cells_x = ctx->num_cells_x;
    sx_rect rect = sx_rect_empty();
    for (int i = 0; i < poly->count; i++) {
        sx_rect_add_point(&rect, poly->verts[i]);
    }
    sx_ivec2 hmin = coll__hash_point(ctx, sx_vec2fv(rect.vmin));
    sx_ivec2 hmax = coll__hash_point(ctx, sx_vec2fv(rect.vmax));
    uint64_t* ents = NULL;

    const sx_alloc* tmp_alloc = the_core->tmp_alloc_push();
    sx_scope(the_core->tmp_alloc_pop()) {
        // broad-phase
        sx_handle_t* candidates = NULL;
        for (int y = hmin.y; y <= hmax.y; y++) {
            for (int x = hmin.x; x <= hmax.x; x++) {
                coll_spatial_grid_cell cell = ctx->cells[coll__cell_id(x, y, num_cells_x)];
                int num_cell_ents = sx_array_count(cell.ents);
                sx_memcpy(sx_array_add(tmp_alloc, candidates, num_cell_ents), cell.ents,
                            num_cell_ents * sizeof(sx_handle_t));
            }
        }

        // remove duplicates
        int num_candidates = sx_array_count(candidates);
        coll__sort_u32_tim_sort(candidates, num_candidates);
        for (int e = 0; e < (num_candidates - 1); e++) {
            if (candidates[e] == candidates[e+1]) {
                sx_array_pop(candidates, e);
                --e;
                --num_candidates;
            }
        }

        sx_array_reserve(alloc, ents, 50);

        for (int e = 0; e < num_candidates; e++) {
            int test_index = sx_handle_index(candidates[e]);
            sx_aabb test_aabb = ctx->transformed_aabbs[test_index];
            sx_rect test_rect = sx_rectv(sx_vec2fv(test_aabb.vmin), sx_vec2fv(test_aabb.vmax));
            coll_entity_mask_pair test_entmask = ctx->ent_mask_pairs[test_index];

            if ((mask & test_entmask.mask) == 0 || !sx_rect_test_rect(rect, test_rect)) {
                continue;
            }

            sx_box test_box = ctx->transformed_boxes[test_index];
            if ((test_box.e.x + test_box.e.y + test_box.e.z) > 0.00001f) {
                c2Poly poly2;
                c2x polytx2;
                coll__calc_poly_from_box(&test_box, &poly2, &polytx2);
                if (!c2PolytoPoly((const c2Poly*)poly, NULL, &poly2, &polytx2)) {
                    continue;
                }
            } else {
                // static poly
                if (!c2PolytoPoly((const c2Poly*)poly, NULL, (const c2Poly*)&ctx->polys[test_index], NULL)) {
                    continue;
                }
            }

            sx_array_push(alloc, ents, test_entmask.entity);
        } // foreach (candidate)
    } // scope

    return ents;
}

static inline float coll_ray_intersect_plane(rizz_coll_ray ray, sx_plane p)
{
    /* put (pt + t*dir) into plane equation and solve t -> (pt + t*dir)*N + d = 0 */
    float v_dot_n = ray.dir.x*p.normal[0] + ray.dir.y*p.normal[1] + ray.dir.z*p.normal[2];
    if (sx_abs(v_dot_n) < 0.000001f)
        return -1.0f;
    float p_dot_n = ray.origin.x*p.normal[0] + ray.origin.y*p.normal[1] + ray.origin.z*p.normal[2];
    float t = -(p_dot_n + p.dist)/v_dot_n;
    return t;    
}

static bool coll_ray_cast_box(sx_box* box, rizz_coll_ray ray, rizz_coll_rayhit* hit)
{
    sx_vec3 d = sx_mat3_mul_vec3_inverse(&box->tx.rot, ray.dir);
    sx_vec3 p = sx_tx3d_mul_vec3_inverse(&box->tx, ray.origin);
    float const epsilon = 1.0e-8f;
    float tmin = 0;
    float tmax = ray.len;
    
    float t0;
    float t1;
    sx_vec3 n0 = SX_VEC3_ZERO;
    sx_vec3 e = box->e;

    for (int i = 0; i < 3; i++) {
        if (sx_abs(d.f[i]) < epsilon) {
            if (p.f[i] < -e.f[i] || p.f[i] > e.f[i]) {
                return false;
            }
        } else {
            float d0 = 1.0f / d.f[i];
            float s = sx_sign(d.f[i]);
            float ei = e.f[i] * s;

            sx_vec3 n = SX_VEC3_ZERO;
            n.f[i] = -s;

            t0 = -(ei + p.f[i]) * d0;
            t1 =  (ei - p.f[i]) * d0;

            if (t0 > tmin) {
                n0 = n;
                tmin = t0;
            }

            tmax = sx_min(tmax, t1);
            if (tmin > tmax) {
                return false;
            }
        }
    }

    if (tmin <= epsilon) {
        return false;
    }

    hit->t = tmin;
    hit->normal = sx_mat3_mul_vec3(&box->tx.rot, n0);

    return true;
}

static inline bool coll__id_exists(const int* arr, int count, int v)
{
    if (!arr) {
        return false;
    }

    for (int i = 0; i < count; i++) {
        if (arr[i] == v) {
            return true;
        }
    }
    return false;
}

static rizz_coll_rayhit* coll_query_ray(rizz_coll_context* ctx, rizz_coll_ray ray, uint32_t mask,
                                        const sx_alloc* alloc)
{
    float const map_size_x = ctx->map_size_x;
    float const map_size_y = ctx->map_size_y;
    int const num_cells_x = ctx->num_cells_x;
    float _t;

    #if STRIKE_DEBUG_COLLISION
        int64_t frame_id = the_core->frame_index(); // used for debugging only
        sx_array_push(ctx->alloc, ctx->rays, ray);
    #endif

    // check ray origin with world boundries and clip it against it if it's outside
    sx_rect map_rect = sx_rectwh(-map_size_x*0.5f, -map_size_y*0.5f, map_size_x, map_size_y);
    if (!sx_rect_test_point(map_rect, sx_vec2fv(ray.origin.f))) {
        if ((_t = coll_ray_intersect_plane(ray, sx_planef(0, 1.0f, 0, -map_size_y*0.5f))) >= 0) {
            if (_t >= ray.len) {
                return NULL;
            }
            ray.origin = sx_vec3_add(ray.origin, sx_vec3_mulf(ray.dir, _t));
        }
        if ((_t = coll_ray_intersect_plane(ray, sx_planef(0, -1.0f, 0, -map_size_y*0.5f))) >= 0) {
            if (_t >= ray.len) {
                return NULL;
            }
            ray.origin = sx_vec3_add(ray.origin, sx_vec3_mulf(ray.dir, _t));
        }
        if ((_t = coll_ray_intersect_plane(ray, sx_planef(1.0f, 0, 0, -map_size_x*0.5f))) >= 0) {
            if (_t >= ray.len) {
                return NULL;
            }
            ray.origin = sx_vec3_add(ray.origin, sx_vec3_mulf(ray.dir, _t));
        }
        if ((_t = coll_ray_intersect_plane(ray, sx_planef(-1.0f, 0, 0, -map_size_x*0.5f))) >= 0) {
            if (_t >= ray.len) {
                return NULL;
            }
            ray.origin = sx_vec3_add(ray.origin, sx_vec3_mulf(ray.dir, _t));
        }
    }

    // intersect the ray with map boundries, so we won't get incorrect broadphase results
    if ((_t = coll_ray_intersect_plane(ray, sx_planef(0, 1.0f, 0, map_size_y*0.5f))) >= 0) {
        ray.len = sx_min(ray.len, _t);
    }
    if ((_t = coll_ray_intersect_plane(ray, sx_planef(0, -1.0f, 0, map_size_y*0.5f))) >= 0) {
        ray.len = sx_min(ray.len, _t);
    }
    if ((_t = coll_ray_intersect_plane(ray, sx_planef(1.0f, 0, 0, map_size_x*0.5f))) >= 0) {
        ray.len = sx_min(ray.len, _t);
    }
    if ((_t = coll_ray_intersect_plane(ray, sx_planef(-1.0f, 0, 0, map_size_x*0.5f))) >= 0) {
        ray.len = sx_min(ray.len, _t);
    }

    // broadphase
    sx_vec2 target = sx_vec2fv(sx_vec3_add(ray.origin, sx_vec3_mulf(ray.dir, ray.len - 0.00001f)).f);
    if (!sx_rect_test_point(map_rect, target)) {
        return NULL;
    }

    sx_vec2 origin2d = sx_vec2fv(ray.origin.f);
    sx_ivec2 p0 = coll__hash_point(ctx, origin2d);
    sx_ivec2 p1 = coll__hash_point(ctx, target);

    // broadphase: Bresenham AA line drawing
    int* candidate_cells = NULL;
    rizz_coll_rayhit* hits = NULL;
    const sx_alloc* tmp_alloc = the_core->tmp_alloc_push();

    sx_scope(the_core->tmp_alloc_pop()) {
        sx_array_reserve(alloc, hits, 50);

        int x0 = p0.x, y0 = p0.y, x1 = p1.x, y1 = p1.y;
        int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
        int dy = abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
        int err = dx - dy, e2, x2;
        int ed = dx + dy == 0 ? 1 : (int)sx_sqrt((float)dx*dx + (float)dy*dy);
        int cell_id;

        while (1) {
            cell_id = coll__cell_id(x0, y0, num_cells_x);
            if (!coll__id_exists(candidate_cells, sx_array_count(candidate_cells), cell_id)) {
                #ifdef STRIKE_DEBUG_COLLISION
                    ++ctx->cells[cell_id].num_raymarches;
                #endif
                sx_array_push(tmp_alloc, candidate_cells, cell_id);
            }
            e2 = err; x2 = x0;
            if (2 * e2 >= -dx) {    // x step
                if (x0 == x1) {
                    break;
                }
                if (e2 + dy < ed) {
                    cell_id = coll__cell_id(x0, y0 + sy, num_cells_x);
                    if (!coll__id_exists(candidate_cells, sx_array_count(candidate_cells), cell_id)) {
                        #ifdef STRIKE_DEBUG_COLLISION
                            ++ctx->cells[cell_id].num_raymarches;
                        #endif
                        sx_array_push(tmp_alloc, candidate_cells, cell_id);
                    }
                }
                err -= dy; x0 += sx;
            }
            if (2 * e2 <= dy) {     // y step
                if (y0 == y1) {
                    break;
                }

                if (dx-e2 < ed) {
                    cell_id = coll__cell_id(x2 + sx, y0, num_cells_x);
                    if (!coll__id_exists(candidate_cells, sx_array_count(candidate_cells), cell_id)) {
                        #ifdef STRIKE_DEBUG_COLLISION
                            ++ctx->cells[cell_id].num_raymarches;
                        #endif
                        sx_array_push(tmp_alloc, candidate_cells, cell_id);
                    }
                } 
                err += dx; y0 += sy;
            }
        }

        // extract entities from cells and sort/remove duplicates
        sx_handle_t* candidates = NULL;
        for (int i = 0, ic = sx_array_count(candidate_cells); i < ic; i++) {
            coll_spatial_grid_cell* cell = &ctx->cells[candidate_cells[i]];
            int num_cell_ents = sx_array_count(cell->ents);
            sx_memcpy(sx_array_add(tmp_alloc, candidates, num_cell_ents), cell->ents,
                      num_cell_ents * sizeof(sx_handle_t));

        }   

        // remove duplicates
        int num_candidates = sx_array_count(candidates);
        coll__sort_u32_tim_sort(candidates, num_candidates);
        for (int i = 0; i < (num_candidates - 1); i++) {
            if (candidates[i] == candidates[i+1]) {
                sx_array_pop(candidates, i);
                --i;
                --num_candidates;
            }
        }

        #if STRIKE_DEBUG_COLLISION
            for (int i = 0; i < num_candidates; i++) {
                int index = sx_handle_index(candidates[i]);
                ctx->raymarch_frames[index] = frame_id;
            }
        #endif // STRIKE_DEBUG_COLLISION

        // perform narrow-phase ray cast on entities
        rizz_coll_rayhit hit;
        for (int i = 0; i < num_candidates; i++) {
            int index = sx_handle_index(candidates[i]);
            sx_box test_box = ctx->transformed_boxes[index];
            coll_entity_mask_pair test_entmask = ctx->ent_mask_pairs[index];

            // must meet 3 conditions:
            //  - masks bits match
            //  - entity is not static poly
            //  - ray intersects with the box
            if ((mask & test_entmask.mask) && (test_box.e.x + test_box.e.y + test_box.e.z) > 0.00001f) {
                if (coll_ray_cast_box(&test_box, ray, &hit)) {
                    #ifdef STRIKE_DEBUG_COLLISION
                        ctx->rayhit_frames[index] = frame_id;
                        coll__mark_rayhit(ctx, &ctx->transformed_aabbs[index]);
                    #endif
                    hit.ent = test_entmask.entity;
                    sx_array_push(alloc, hits, hit);
                } 
            } // if (mask) 
        } // foreach (candidates)

        // now sort all results by closest to the ray origin
        int hit_count = sx_array_count(hits);
        if (hit_count > 0) {
            coll__sort_rayhit_tim_sort(hits, hit_count);
        }
    } // scope

    return hits;
}

static int coll_num_cells(rizz_coll_context* ctx, int* num_cells_x, int* num_cells_y)
{
    if (num_cells_x) {
        *num_cells_x = ctx->num_cells_x;
    }
    if (num_cells_y) {
        *num_cells_y = ctx->num_cells_y;
    }
    return ctx->num_cells;
}

static sx_rect coll_cell_rect(rizz_coll_context* ctx, int cell_idx)
{
    sx_assert(cell_idx < ctx->num_cells);
    return sx_rectce(ctx->cells[cell_idx].center, sx_vec2splat(ctx->grid_cell_size*0.5f));
}

static bool coll_get_entity_data(rizz_coll_context* ctx, uint64_t ent, rizz_coll_entity_data* outdata)
{
    sx_handle_t handle = (sx_handle_t)sx_hashtbl_find_get(ctx->ent_tbl, sx_hash_u64_to_u32(ent), 0);
    if (handle) {
        sx_assert(sx_handle_valid(ctx->handles, handle));

        int index = sx_handle_index(handle);
        sx_vec3 e = ctx->boxes[index].e;
        outdata->box = ctx->transformed_boxes[index];
        outdata->aabb = ctx->transformed_aabbs[index];
        outdata->is_static = (e.x + e.y + e.z) < 0.00001f;
        outdata->poly = ctx->polys[index];
        outdata->mask = ctx->ent_mask_pairs[index].mask;

        return true;
    } else {
        return false;
    }
}

#if STRIKE_DEBUG_COLLISION
static void coll__box_to_verts(const sx_box* box, sx_vec3 verts[4])
{
    verts[0] = sx_tx3d_mul_vec3(&box->tx, sx_vec3f( box->e.x,  box->e.y, 0));
    verts[1] = sx_tx3d_mul_vec3(&box->tx, sx_vec3f(-box->e.x,  box->e.y, 0));
    verts[2] = sx_tx3d_mul_vec3(&box->tx, sx_vec3f(-box->e.x, -box->e.y, 0));
    verts[3] = sx_tx3d_mul_vec3(&box->tx, sx_vec3f( box->e.x, -box->e.y, 0));    
}

static void coll_debug_collisions(rizz_coll_context* ctx, float opacity,
                                  rizz_coll_debug_collision_mode mode, float heatmap_limit)
{
    ImDrawList* draw_list = the_imguix->begin_fullscreen_draw("strike_coll_collisions");

    sx_vec2 wsize;
    the_app->window_size(&wsize);
    // dim background
    the_imgui->ImDrawList_AddRectFilled(draw_list, SX_VEC2_ZERO, wsize,
                                        sx_color4u(0, 0, 0, (uint8_t)(opacity*255.0f)).n, 0, 0);

    sx_rect map_rect = sx_rectwh(-ctx->map_size_x * 0.5f, -ctx->map_size_y * 0.5f,
                                 ctx->map_size_x, ctx->map_size_y);
    sx_rect viewport = sx_rect_expand(
        map_rect, sx_vec2_mulf(sx_vec2f(ctx->map_size_x, ctx->map_size_y), 0.05f));
    sx_mat4 proj = sx_mat4_ortho_offcenter(
        viewport.xmin, viewport.ymin, viewport.xmax, viewport.ymax,
        -10.0f, 10.0f, 0, the_gfx->GL_family());
    sx_mat4 view = sx_mat4_view_lookat(sx_vec3f(0, 0.0f, 5.0f), SX_VEC3_ZERO, SX_VEC3_UNITY);
    sx_mat4 viewproj = sx_mat4_mul(&proj, &view);

    // draw heat map grid
    switch (mode) {
    case RIZZ_COLL_DEBUGCOLLISION_MODE_COLLISIONS: {
        int64_t frame = the_core->frame_index();
        sx_box box;
        sx_vec3 verts[4];

        for (int i = 0, ic = ctx->handles->count; i < ic; i++) {
            sx_handle_t handle = sx_handle_at(ctx->handles, i);
            int index = sx_handle_index(handle);

            box = ctx->transformed_boxes[index];
            coll__box_to_verts(&box, verts);

            the_imgui->ImDrawList_AddQuad(
                draw_list, the_imguix->project_to_screen(verts[0], &viewproj, NULL),
                the_imguix->project_to_screen(verts[1], &viewproj, NULL),
                the_imguix->project_to_screen(verts[2], &viewproj, NULL),
                the_imguix->project_to_screen(verts[3], &viewproj, NULL),
                ctx->collision_frames[index] == frame ? SX_COLOR_RED.n : SX_COLOR_WHITE.n, 1.0f);
        }
    } break;
    case RIZZ_COLL_DEBUGCOLLISION_MODE_COLLISION_HEATMAP:
    case RIZZ_COLL_DEBUGCOLLISION_MODE_ENTITY_HEATMAP: {
        float base_color[3] = { 0, 1.0f, 0 };
        float base_hsv[3];
        sx_color_RGBtoHSV(base_hsv, base_color);

        for (int i = 0; i < ctx->num_cells; i++) {
            coll_spatial_grid_cell* cell = &ctx->cells[i];
            sx_rect cell_rect = sx_rectce(cell->center, sx_vec2splat(ctx->grid_cell_size * 0.5f));
            sx_vec2 v1 = the_imguix->project_to_screen(sx_vec3v2(sx_vec2fv(cell_rect.vmin), 0), &viewproj, NULL);
            sx_vec2 v2 = the_imguix->project_to_screen(sx_vec3v2(sx_vec2fv(cell_rect.vmax), 0), &viewproj, NULL);
            sx_swap(v1.y, v2.y, float);

            float count_ratio;
            if (mode == RIZZ_COLL_DEBUGCOLLISION_MODE_COLLISION_HEATMAP) {
                count_ratio = (float)sx_array_count(cell->ents) / heatmap_limit;
            } else {
                count_ratio = (float)cell->num_collisions / heatmap_limit;
            }
            count_ratio = sx_min(1.0f, count_ratio);
            float hsv[3] = { sx_lerp(base_hsv[0], 0, count_ratio), base_hsv[1], base_hsv[2] };
            float color[3];
            sx_color_HSVtoRGB(color, hsv);

            the_imgui->ImDrawList_AddRectFilled(
                draw_list, v1, v2, sx_color4f(color[0], color[1], color[2], 0.3f).n, 0, 0);
        }
    } break;
    } // switch

    // world bounds
    the_imgui->ImDrawList_AddRect(draw_list, 
        the_imguix->project_to_screen(sx_vec3v2(sx_vec2fv(map_rect.vmin), 0), &viewproj, NULL),
        the_imguix->project_to_screen(sx_vec3v2(sx_vec2fv(map_rect.vmax), 0), &viewproj, NULL),
        SX_COLOR_YELLOW.n, 0, 0, 2.0f);
}

static void coll_debug_raycast(rizz_coll_context* ctx, float opacity,
                               rizz_coll_debug_raycast_mode mode, float heatmap_limit)
{
    ImDrawList* draw_list = the_imguix->begin_fullscreen_draw("strike_coll_raycast");

    float map_size_x = ctx->map_size_x;
    float map_size_y = ctx->map_size_y;
    
    // dim background
    sx_vec2 wsize;
    the_app->window_size(&wsize);
    the_imgui->ImDrawList_AddRectFilled(draw_list, SX_VEC2_ZERO, wsize,
                                        sx_color4u(0, 0, 0, (uint8_t)(opacity*255.0f)).n, 0, 0);

    sx_rect map_rect = sx_rectwh(-map_size_x*0.5f, -map_size_y*0.5f, map_size_x, map_size_y);
    sx_rect viewport = sx_rect_expand(map_rect, sx_vec2_mulf(sx_vec2f(map_size_x, map_size_y), 0.05f));
    sx_mat4 proj = sx_mat4_ortho_offcenter(
        viewport.xmin, viewport.ymin, viewport.xmax, viewport.ymax,
        -10.0f, 10.0f, 0, the_gfx->GL_family());
    sx_mat4 view = sx_mat4_view_lookat(sx_vec3f(0, 0.0f, 5.0f), SX_VEC3_ZERO, SX_VEC3_UNITY);
    sx_mat4 viewproj = sx_mat4_mul(&proj, &view);

    // draw heat map grid
    switch (mode) {
    case RIZZ_COLL_DEBUGRAYCAST_MODE_RAYHITS: {
        int64_t frame = the_core->frame_index();
        sx_box box;
        sx_vec3 verts[4];

        for (int i = 0, ic = ctx->handles->count; i < ic; i++) {
            sx_handle_t handle = sx_handle_at(ctx->handles, i);
            int index = sx_handle_index(handle);

            box = ctx->transformed_boxes[index];
            coll__box_to_verts(&box, verts);

            sx_color color = SX_COLOR_WHITE;
            if (ctx->rayhit_frames[index] == frame) {
                color = SX_COLOR_RED;
            } else if (ctx->raymarch_frames[index] == frame) {
                color = SX_COLOR_GREEN;
            }

            the_imgui->ImDrawList_AddQuad(
                draw_list, the_imguix->project_to_screen(verts[0], &viewproj, NULL),
                the_imguix->project_to_screen(verts[1], &viewproj, NULL),
                the_imguix->project_to_screen(verts[2], &viewproj, NULL),
                the_imguix->project_to_screen(verts[3], &viewproj, NULL),
                color.n, 1.0f);
        }
    } break;
    case RIZZ_COLL_DEBUGRAYCAST_MODE_RAYHIT_HEATMAP:
    case RIZZ_COLL_DEBUGRAYCAST_MODE_RAYMARCH_HEATMAP: {
        float base_color[3] = { 0, 1.0f, 0 };
        float base_hsv[3];
        sx_color_RGBtoHSV(base_hsv, base_color);

        for (int i = 0; i < ctx->num_cells; i++) {
            coll_spatial_grid_cell* cell = &ctx->cells[i];
            sx_rect cell_rect = sx_rectce(cell->center, sx_vec2splat(ctx->grid_cell_size * 0.5f));
            sx_vec2 v1 = the_imguix->project_to_screen(sx_vec3v2(sx_vec2fv(cell_rect.vmin), 0), &viewproj, NULL);
            sx_vec2 v2 = the_imguix->project_to_screen(sx_vec3v2(sx_vec2fv(cell_rect.vmax), 0), &viewproj, NULL);
            sx_swap(v1.y, v2.y, float);

            float count_ratio;
            if (mode == RIZZ_COLL_DEBUGRAYCAST_MODE_RAYHIT_HEATMAP) {
                count_ratio = (float)cell->num_rayhits / heatmap_limit;
            } else {
                count_ratio = (float)cell->num_raymarches / heatmap_limit;
            }
            count_ratio = sx_min(1.0f, count_ratio);
            float hsv[3] = { sx_lerp(base_hsv[0], 0, count_ratio), base_hsv[1], base_hsv[2] };
            float color[3];
            sx_color_HSVtoRGB(color, hsv);

            the_imgui->ImDrawList_AddRect(draw_list, v1, v2, SX_COLOR_GREEN.n, 0, ImDrawFlags_None, 1.0f);
            the_imgui->ImDrawList_AddRectFilled(
                draw_list, v1, v2, sx_color4f(color[0], color[1], color[2], 0.3f).n, 0, 0);
        }
    } break;
    } // switch

    // world bounds
    the_imgui->ImDrawList_AddRect(draw_list, 
        the_imguix->project_to_screen(sx_vec3v2(sx_vec2fv(map_rect.vmin), 0), &viewproj, NULL),
        the_imguix->project_to_screen(sx_vec3v2(sx_vec2fv(map_rect.vmax), 0), &viewproj, NULL),
        SX_COLOR_YELLOW.n, 0, 0, 2.0f);    

    // rays
    for (int i = 0, ic = sx_array_count(ctx->rays); i < ic; i++) {
        rizz_coll_ray ray = ctx->rays[i];
        sx_vec2 ray_pos = the_imguix->project_to_screen(ray.origin, &viewproj, NULL);
        float len = ray.len;
        {
            float _t;
            if ((_t = coll_ray_intersect_plane(ray, sx_planef(0, 1.0f, 0, map_size_y*0.5f))) >= 0) {
                len = sx_min(len, _t);
            }
            if ((_t = coll_ray_intersect_plane(ray, sx_planef(0, -1.0f, 0, map_size_y*0.5f))) >= 0) {
                len = sx_min(len, _t);
            }
            if ((_t = coll_ray_intersect_plane(ray, sx_planef(1.0f, 0, 0, map_size_x*0.5f))) >= 0) {
                len = sx_min(len, _t);
            }
            if ((_t = coll_ray_intersect_plane(ray, sx_planef(-1.0f, 0, 0, map_size_x*0.5f))) >= 0) {
                len = sx_min(len, _t);
            }
        }    
        
        sx_vec2 ray_end = the_imguix->project_to_screen(
            sx_vec3_add(ray.origin, sx_vec3_mulf(ray.dir, len)), &viewproj, NULL);
        the_imgui->ImDrawList_AddCircle(draw_list, ray_pos, 5.0f, SX_COLOR_PURPLE.n, 12, 2.0f);
        the_imgui->ImDrawList_AddLine(draw_list, ray_pos, ray_end, SX_COLOR_PURPLE.n, 2.0f);    
    }

    for (int i = 0; i < ctx->num_cells; i++) {
        ctx->cells[i].num_raymarches = 0;
        ctx->cells[i].num_rayhits = 0;
    }
    sx_array_clear(ctx->rays);
}
#else // STRIKE_DEBUG_COLLISION==0
void coll_debug_collisions(rizz_coll_context* ctx, float opacity,
                           rizz_coll_debug_collision_mode mode, float heatmap_limit)
{
    sx_unused(mode);
    sx_unused(ctx);
    sx_unused(opacity);
    sx_unused(heatmap_limit);
}

void coll_debug_raycast(rizz_coll_context* ctx, float opacity, rizz_coll_debug_collision_mode mode,
                        float heatmap_limit)
{
    sx_unused(opacity);
    sx_unused(mode);
    sx_unused(ctx);
    sx_unused(heatmap_limit);

}
#endif // STRIKE_DEBUG_COLLISION

static rizz_api_coll the__coll = {
    .create_context = coll_create_context,
    .destroy_context = coll_destroy_context,
    .add_boxes = coll_add_boxes,
    .add_static_polys = coll_add_static_polys,
    .remove = coll_remove,
    .remove_all = coll_remove_all,
    .update_transforms = coll_update_transforms,
    .detect = coll_detect,
    .query_sphere = coll_query_sphere,
    .query_poly = coll_query_poly,
    .query_ray = coll_query_ray,
    .debug_collisions = coll_debug_collisions,
    .debug_raycast = coll_debug_raycast,
    .num_cells = coll_num_cells,
    .cell_rect = coll_cell_rect,
    .get_entity_data = coll_get_entity_data
};

rizz_plugin_decl_main(collision, plugin, e)
{
    switch (e) {
    case RIZZ_PLUGIN_EVENT_STEP:
        break;
    case RIZZ_PLUGIN_EVENT_INIT:
        the_plugin = plugin->api;
        the_core = the_plugin->get_api(RIZZ_API_CORE, 0);
        the_app = the_plugin->get_api(RIZZ_API_APP, 0);
        the_imgui = the_plugin->get_api_byname("imgui", 0);
        the_imguix = the_plugin->get_api_byname("imgui_extra", 0);
        the_gfx = the_plugin->get_api(RIZZ_API_GFX, 0);

        the_plugin->inject_api("collision", 0, &the__coll);
        break;
    case RIZZ_PLUGIN_EVENT_LOAD:
        the_plugin->inject_api("collision", 0, &the__coll);
        break;
    case RIZZ_PLUGIN_EVENT_UNLOAD:
        break;
    case RIZZ_PLUGIN_EVENT_SHUTDOWN:
        the_plugin->remove_api("collision", 0);
        break;
    }

    return 0;
}

rizz_plugin_decl_event_handler(collision, e)
{
    if (e->type == RIZZ_APP_EVENTTYPE_UPDATE_APIS) {
        the_imgui = the_plugin->get_api_byname("imgui", 0);
        the_imguix = the_plugin->get_api_byname("imgui_extra", 0);
    }
}


static const char* collision__deps[] = { "imgui" };
rizz_plugin_implement_info(collision, 1000, "collision plugin", collision__deps, 1);

