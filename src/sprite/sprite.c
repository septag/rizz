#include "sprite/sprite.h"

#include "sx/allocator.h"
#include "sx/array.h"
#include "sx/handle.h"
#include "sx/hash.h"
#include "sx/os.h"
#include "sx/string.h"

#include "rizz/asset.h"
#include "rizz/core.h"
#include "rizz/graphics.h"
#include "rizz/json.h"
#include "rizz/plugin.h"
#include "rizz/reflect.h"

#include <alloca.h>

static rizz_api_core*   the_core;
static rizz_api_plugin* the_plugin;
static rizz_api_asset*  the_asset;
static rizz_api_refl*   the_refl;

typedef struct {
    sx_str_t         name;
    rizz_asset       atlas;
    int              atlas_sprite_id;
    rizz_asset       texture;
    sx_vec2          size;    // size set by API (x or y can be <= 0)
    sx_vec2          origin;
    sx_color         color;
    rizz_sprite_flip flip;
    sx_rect          bounds;
} sprite__data;

typedef struct {
    sx_vec2 base_size;
    sx_rect sprite_rect;
    sx_rect sheet_rect;
    int     num_indices;
    int     num_verts;
    int     ib_index;
    int     vb_index;
} atlas__sprite;

typedef struct {
    rizz_atlas          a;
    atlas__sprite*      sprites;
    sx_hashtbl          sprite_tbl;    // key: name, value:index-to-sprites
    rizz_sprite_vertex* vertices;
    uint16_t*           indices;
} atlas__data;

typedef struct {
    char img_filepath[RIZZ_MAX_PATH];
    int  num_sprites;
    int  num_vertices;
    int  num_indices;
} atlas__metadata;

typedef struct {
    const sx_alloc* alloc;
    sx_strpool*     name_pool;
    sx_handle_pool* sprite_handles;
    sprite__data*   sprites;
} sprite__context;

#define SORT_NAME sprite__sort
#define SORT_TYPE uint64_t
#define SORT_CMP(x, y) ((x) < (y) ? -1 : 1)
SX_PRAGMA_DIAGNOSTIC_PUSH()
SX_PRAGMA_DIAGNOSTIC_IGNORED_MSVC(4267)
SX_PRAGMA_DIAGNOSTIC_IGNORED_MSVC(4244)
SX_PRAGMA_DIAGNOSTIC_IGNORED_MSVC(4146)
SX_PRAGMA_DIAGNOSTIC_IGNORED_CLANG_GCC("-Wunused-function")
#include "sort/sort.h"
SX_PRAGMA_DIAGNOSTIC_POP()

static sprite__context g_spr;

static sx_vec2 sprite__calc_size(const sx_vec2 size, const sx_vec2 base_size,
                                 rizz_sprite_flip flip) {
    sx_assert(size.x > 0 || size.y > 0);
    sx_vec2 _size;
    // if any component of half-size is set to less than zero, then it will be evaluated by ratio
    if (size.y <= 0) {
        float ratio = base_size.y / base_size.x;
        _size = sx_vec2f(size.x, size.x * ratio);
    } else if (size.x <= 0) {
        float ratio = base_size.x / base_size.y;
        _size = sx_vec2f(size.x * ratio, size.y);
    } else {
        _size = size;
    }

    // flip
    if (flip & RIZZ_SPRITE_FLIP_X)
        _size.x = -_size.x;
    if (flip & RIZZ_SPRITE_FLIP_Y)
        _size.y = -_size.y;

    return _size;
}

static inline sx_vec2 sprite__normalize_pos(const sx_vec2 pos_px, const sx_vec2 base_size_rcp) {
    sx_vec2 n = sx_vec2_mul(pos_px, base_size_rcp);
    return sx_vec2f(n.x - 0.5f, 0.5f - n.y);    // flip-y and transform to (0.5f, 0.5f) space
}

static inline void sprite__calc_coords(sx_vec2* points, int num_points, const sx_vec2* points_px,
                                       const sx_vec2 size, const sx_vec2 origin) {
    const sx_vec2 size_rcp = sx_vec2f(1.0f / size.x, 1.0f / size.y);
    for (int i = 0; i < num_points; i++) {
        sx_vec2 n = sx_vec2_mul(points_px[i], size_rcp);    // normalize to 0..1
        n = sx_vec2f(n.x - 0.5f, 0.5f - n.y);    // flip-y and transform to (0.5f, 0.5f) space
        points[i] = sx_vec2_mul(sx_vec2_sub(n, origin), size);    // offset by origin and resize
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// atlas
static rizz_asset_load_data atlas__on_prepare(const rizz_asset_load_params* params,
                                              const void*                   metadata) {
    const sx_alloc*        alloc = params->alloc ? params->alloc : g_spr.alloc;
    const atlas__metadata* meta = metadata;
    sx_assert(meta);

    int hashtbl_cap = sx_hashtbl_valid_capacity(meta->num_sprites);
    int total_sz = sizeof(atlas__data) + meta->num_sprites * sizeof(atlas__sprite) +
                   meta->num_indices * sizeof(uint16_t) +
                   meta->num_vertices * sizeof(rizz_sprite_vertex) +
                   sx_hashtbl_fixed_size(meta->num_sprites);
    atlas__data* atlas = sx_malloc(alloc, total_sz);
    if (!atlas) {
        sx_out_of_memory();
        return (rizz_asset_load_data){ .obj = { 0 } };
    }
    sx_memset(atlas, 0x0, total_sz);

    uint8_t* buff = (uint8_t*)(atlas + 1);
    atlas->sprites = (atlas__sprite*)buff;
    buff += sizeof(atlas__sprite) * meta->num_sprites;
    uint32_t* keys = (uint32_t*)buff;
    buff += sizeof(uint32_t) * hashtbl_cap;
    int* values = (int*)buff;
    buff += sizeof(int) * hashtbl_cap;
    sx_hashtbl_init(&atlas->sprite_tbl, hashtbl_cap, keys, values);
    atlas->vertices = (rizz_sprite_vertex*)buff;
    buff += sizeof(rizz_sprite_vertex) * meta->num_vertices;
    atlas->indices = (uint16_t*)buff;
    buff += sizeof(uint16_t) * meta->num_indices;

    rizz_texture_load_params tparams = { .wrap_u = SG_WRAP_CLAMP_TO_EDGE,
                                         .wrap_v = SG_WRAP_CLAMP_TO_EDGE };
    atlas->a.texture = the_asset->load("texture", meta->img_filepath, &tparams, params->flags,
                                       alloc, params->tags);

    return (rizz_asset_load_data){ .obj = { .ptr = atlas }, .user = buff };
}

static bool atlas__on_load(rizz_asset_load_data* data, const rizz_asset_load_params* params,
                           const sx_mem_block* mem) {
    atlas__data* atlas = data->obj.ptr;

    const sx_alloc* tmp_alloc = the_core->tmp_alloc_push();

    char* buff = sx_malloc(tmp_alloc, mem->size + 1);
    if (!buff) {
        sx_out_of_memory();
        return false;
    }

    sx_memcpy(buff, mem->data, mem->size);
    buff[mem->size] = '\0';
    sjson_context* jctx = sjson_create_context(0, 0, (void*)tmp_alloc);
    sx_assert(jctx);

    sjson_node* jroot = sjson_decode(jctx, buff);
    if (!jroot) {
        rizz_log_warn(the_core, "loading atlas '%s' failed: not a valid json file", params->path);
        return false;
    }

    atlas->a.info.img_width = sjson_get_int(jroot, "image_width", 0);
    atlas->a.info.img_height = sjson_get_int(jroot, "image_height", 0);
    int         sprite_idx = 0;
    sjson_node* jsprites = sjson_find_member(jroot, "sprites");
    sjson_node* jsprite;
    sx_assert(jsprites);
    sx_vec2 atlas_size = sx_vec2f((float)atlas->a.info.img_width, (float)atlas->a.info.img_height);
    sx_vec2 atlas_size_rcp = sx_vec2f(1.0f / atlas_size.x, 1.0f / atlas_size.y);
    int     ib_index = 0;
    int     vb_index = 0;
    sjson_foreach(jsprite, jsprites) {
        int            tmp[4];
        atlas__sprite* aspr = &atlas->sprites[sprite_idx];
        const char*    name = sjson_get_string(jsprite, "name", "");
        sx_hashtbl_add(&atlas->sprite_tbl, sx_hash_fnv32_str(name), sprite_idx);

        sjson_get_ints(tmp, 2, jsprite, "size");
        aspr->base_size = sx_vec2f((float)tmp[0], (float)tmp[1]);

        sjson_get_ints(tmp, 4, jsprite, "sprite_rect");
        aspr->sprite_rect = sx_rectf((float)tmp[0], (float)tmp[1], (float)tmp[2], (float)tmp[3]);

        sjson_get_ints(tmp, 4, jsprite, "sheet_rect");
        aspr->sheet_rect = sx_rectf((float)tmp[0], (float)tmp[1], (float)tmp[2], (float)tmp[3]);

        // load geometry
        sx_vec2     base_size_rcp = sx_vec2f(1.0f / aspr->base_size.x, 1.0f / aspr->base_size.y);
        sjson_node* jmesh = sjson_find_member(jsprite, "mesh");
        if (jmesh) {
            // sprite-mesh
            aspr->num_indices = sjson_get_int(jmesh, "num_tris", 0) * 3;
            aspr->num_verts = sjson_get_int(jmesh, "num_vertices", 0);
            rizz_sprite_vertex* verts = &atlas->vertices[vb_index];
            uint16_t*           indices = &atlas->indices[ib_index];
            sjson_get_uint16s(indices, aspr->num_indices, jmesh, "indices");
            sjson_node* jposs = sjson_find_member(jmesh, "positions");
            if (jposs) {
                sjson_node* jpos;
                int         v = 0;
                sjson_foreach(jpos, jposs) {
                    sx_vec2 pos;
                    sjson_get_floats(pos.f, 2, jpos, NULL);
                    sx_assert(v < aspr->num_verts);
                    verts[v].pos = sprite__normalize_pos(pos, base_size_rcp);
                    v++;
                }
            }
            sjson_node* juvs = sjson_find_member(jmesh, "uvs");
            if (juvs) {
                sjson_node* juv;
                int         v = 0;
                sjson_foreach(juv, jposs) {
                    sx_vec2 uv;
                    sjson_get_floats(uv.f, 2, juv, NULL);
                    sx_assert(v < aspr->num_verts);
                    verts[v].uv = sx_vec2_mul(uv, atlas_size_rcp);
                    v++;
                }
            }

        } else {
            // sprite-quad
            aspr->num_indices = 6;
            aspr->num_verts = 4;
            rizz_sprite_vertex* verts = &atlas->vertices[vb_index];
            uint16_t*           indices = &atlas->indices[ib_index];
            sx_rect uv_rect = sx_rectv(sx_vec2_mul(aspr->sheet_rect.vmin, atlas_size_rcp),
                                       sx_vec2_mul(aspr->sheet_rect.vmax, atlas_size_rcp));
            verts[0].pos =
                sprite__normalize_pos(sx_rect_corner(&aspr->sprite_rect, 0), base_size_rcp);
            verts[0].uv = sx_rect_corner(&uv_rect, 0);
            verts[1].pos =
                sprite__normalize_pos(sx_rect_corner(&aspr->sprite_rect, 1), base_size_rcp);
            verts[1].uv = sx_rect_corner(&uv_rect, 1);
            verts[2].pos =
                sprite__normalize_pos(sx_rect_corner(&aspr->sprite_rect, 2), base_size_rcp);
            verts[2].uv = sx_rect_corner(&uv_rect, 2);
            verts[3].pos =
                sprite__normalize_pos(sx_rect_corner(&aspr->sprite_rect, 3), base_size_rcp);
            verts[3].uv = sx_rect_corner(&uv_rect, 3);

            // clang-format off
            indices[0] = 0;         indices[1] = 3;     indices[2] = 1;
            indices[3] = 0;         indices[4] = 2;     indices[5] = 3;
            // clang-format on
        }

        aspr->ib_index = ib_index;
        aspr->vb_index = vb_index;
        ib_index += aspr->num_indices;
        vb_index += aspr->num_verts;
        ++sprite_idx;
    }
    atlas->a.info.num_sprites = sprite_idx;

    the_core->tmp_alloc_pop();

    return true;
}

static void atlas__on_finalize(rizz_asset_load_data* data, const rizz_asset_load_params* params,
                               const sx_mem_block* mem) {
    sx_unused(data);
    sx_unused(params);
    sx_unused(mem);
}

static void atlas__on_reload(rizz_asset handle, rizz_asset_obj prev_obj, const sx_alloc* alloc) {
    sx_unused(handle);
    sx_unused(prev_obj);
    sx_unused(alloc);
}

static void atlas__on_release(rizz_asset_obj obj, const sx_alloc* alloc) {
    atlas__data* atlas = obj.ptr;
    sx_assert(atlas);

    if (!alloc)
        alloc = g_spr.alloc;

    if (atlas->a.texture.id) {
        the_asset->unload(atlas->a.texture);
    }

    sx_free(alloc, atlas);
}

static void atlas__on_read_metadata(void* metadata, const rizz_asset_load_params* params,
                                    const sx_mem_block* mem) {
    atlas__metadata* meta = metadata;
    const sx_alloc*  tmp_alloc = the_core->tmp_alloc_push();

    char* buff = sx_malloc(tmp_alloc, mem->size + 1);
    if (!buff) {
        sx_out_of_memory();
        return;
    }
    sx_memcpy(buff, mem->data, mem->size);
    buff[mem->size] = '\0';
    sjson_context* jctx = sjson_create_context(0, 0, (void*)tmp_alloc);
    sx_assert(jctx);

    sjson_node* jroot = sjson_decode(jctx, buff);
    if (!jroot) {
        rizz_log_warn(the_core, "loading atlas '%s' failed: not a valid json file", params->path);
        return;
    }

    char dirname[RIZZ_MAX_PATH];
    sx_os_path_dirname(dirname, sizeof(dirname), params->path);
    sx_os_path_join(meta->img_filepath, sizeof(meta->img_filepath), dirname,
                    sjson_get_string(jroot, "image", ""));
    sx_os_path_unixpath(meta->img_filepath, sizeof(meta->img_filepath), meta->img_filepath);

    sjson_node* jsprites = sjson_find_member(jroot, "sprites");
    sjson_node* jsprite;
    int         num_sprites = 0, num_indices = 0, num_vertices = 0;
    sjson_foreach(jsprite, jsprites) {
        sjson_node* jmesh = sjson_find_member(jsprite, "mesh");
        if (jmesh) {
            num_indices += 3 * sjson_get_int(jmesh, "num_tris", 0);
            num_vertices += sjson_get_int(jmesh, "num_vertices", 0);
        } else {
            num_indices += 6;
            num_vertices += 4;
        }
        ++num_sprites;
    }
    meta->num_sprites = num_sprites;
    meta->num_indices = num_indices;
    meta->num_vertices = num_vertices;

    the_core->tmp_alloc_pop();
}

static bool sprite__init() {
    g_spr.alloc = the_core->alloc(RIZZ_MEMID_GRAPHICS);
    g_spr.name_pool = sx_strpool_create(
        g_spr.alloc, &(sx_strpool_config){ .counter_bits = SX_CONFIG_HANDLE_GEN_BITS,
                                           .index_bits = 32 - SX_CONFIG_HANDLE_GEN_BITS,
                                           .entry_capacity = 4096,
                                           .block_capacity = 32,
                                           .block_sz_kb = 64,
                                           .min_str_len = 23 });

    if (!g_spr.name_pool) {
        sx_out_of_memory();
        return false;
    }

    g_spr.sprite_handles = sx_handle_create_pool(g_spr.alloc, 256);
    sx_assert(g_spr.sprite_handles);

    // register "atlas" asset type and metadata
    rizz_refl_field(the_refl, atlas__metadata, char[RIZZ_MAX_PATH], img_filepath, "img_filepath");
    rizz_refl_field(the_refl, atlas__metadata, int, num_sprites, "num_sprites");
    rizz_refl_field(the_refl, atlas__metadata, int, num_vertices, "num_vertices");
    rizz_refl_field(the_refl, atlas__metadata, int, num_indices, "num_indices");

    the_asset->register_asset_type(
        "atlas",
        (rizz_asset_callbacks){ .on_prepare = atlas__on_prepare,
                                .on_load = atlas__on_load,
                                .on_finalize = atlas__on_finalize,
                                .on_reload = atlas__on_reload,
                                .on_release = atlas__on_release,
                                .on_read_metadata = atlas__on_read_metadata },
        NULL, 0, "atlas__metadata", sizeof(atlas__metadata), (rizz_asset_obj){ .ptr = NULL },
        (rizz_asset_obj){ .ptr = NULL });

    return true;
}

static void sprite__release() {
    if (!g_spr.alloc) {
        return;
    }

    if (g_spr.sprite_handles) {
        sx_handle_destroy_pool(g_spr.sprite_handles, g_spr.alloc);
    }

    if (g_spr.name_pool) {
        sx_strpool_destroy(g_spr.name_pool, g_spr.alloc);
    }

    sx_array_free(g_spr.alloc, g_spr.sprites);

    the_asset->unregister_asset_type("atlas");
}

static rizz_sprite sprite__create(const rizz_sprite_desc* desc) {
    sx_assert(desc->texture.id);

    sx_handle_t handle = sx_handle_new_and_grow(g_spr.sprite_handles, g_spr.alloc);
    sx_assert(handle);
    int          name_len = sx_strlen(desc->name);
    sprite__data spr = { .name = sx_strpool_add(g_spr.name_pool, desc->name, name_len),
                         .size = desc->size,
                         .origin = desc->origin,
                         .color = desc->color,
                         .flip = desc->flip };

    const char* img_type = the_asset->type_name(desc->texture);
    if (sx_strequal(img_type, "texture")) {
        spr.texture = desc->texture;
        spr.atlas_sprite_id = -1;
    } else if (sx_strequal(img_type, "atlas")) {
        spr.atlas = desc->atlas;
        atlas__data* atlas = the_asset->obj(desc->atlas).ptr;
        spr.texture = atlas->a.texture;
        int sidx = sx_hashtbl_find(&atlas->sprite_tbl, sx_hash_fnv32(desc->name, name_len));
        if (sidx != -1) {
            spr.atlas_sprite_id = sx_hashtbl_get(&atlas->sprite_tbl, sidx);
        } else {
            rizz_log_warn(the_core, "sprite not found: '%s' in '%s'", desc->name,
                          the_asset->path(desc->atlas));
        }
    } else {
        sx_assert(0 && "desc->atlas != atlas or desc->texture != texture");
        return (rizz_sprite){ 0 };
    }


    int index = sx_handle_index(handle);
    if (index >= sx_array_count(g_spr.sprites)) {
        sx_array_push(g_spr.alloc, g_spr.sprites, spr);
    } else {
        g_spr.sprites[index] = spr;
    }

    return (rizz_sprite){ handle };
}

static void sprite__destroy(rizz_sprite handle) {
    sx_assert(sx_handle_valid(g_spr.sprite_handles, handle.id));

    sprite__data* spr = &g_spr.sprites[sx_handle_index(handle.id)];
    if (spr->atlas.id) {
        the_asset->unload(spr->atlas);
    } else if (spr->texture.id) {
        the_asset->unload(spr->texture);
    }

    if (spr->name) {
        sx_strpool_del(g_spr.name_pool, spr->name);
    }

    sx_handle_del(g_spr.sprite_handles, handle.id);
}

static sx_vec2 sprite__size(rizz_sprite handle) {
    sx_assert(sx_handle_valid(g_spr.sprite_handles, handle.id));
    sprite__data* spr = &g_spr.sprites[sx_handle_index(handle.id)];
    return spr->size;
}

static sx_vec2 sprite__origin(rizz_sprite handle) {
    sx_assert(sx_handle_valid(g_spr.sprite_handles, handle.id));
    sprite__data* spr = &g_spr.sprites[sx_handle_index(handle.id)];
    return spr->origin;
}

static sx_color sprite__color(rizz_sprite handle) {
    sx_assert(sx_handle_valid(g_spr.sprite_handles, handle.id));
    sprite__data* spr = &g_spr.sprites[sx_handle_index(handle.id)];
    return spr->color;
}

static const char* sprite__name(rizz_sprite handle) {
    sx_assert(sx_handle_valid(g_spr.sprite_handles, handle.id));
    sprite__data* spr = &g_spr.sprites[sx_handle_index(handle.id)];
    return sx_strpool_cstr(g_spr.name_pool, spr->name);
}

static sx_rect sprite__bounds(rizz_sprite handle) {
    sx_assert(0);
    return sx_rectf(0, 0, 0, 0);
}

static rizz_sprite_flip sprite__flip(rizz_sprite handle) {
    sx_assert(sx_handle_valid(g_spr.sprite_handles, handle.id));
    sprite__data* spr = &g_spr.sprites[sx_handle_index(handle.id)];
    return spr->flip;
}

static void sprite__set_size(rizz_sprite handle, const sx_vec2 size) {
    sx_assert(sx_handle_valid(g_spr.sprite_handles, handle.id));
    sprite__data* spr = &g_spr.sprites[sx_handle_index(handle.id)];
    spr->size = size;
}

static void sprite__set_origin(rizz_sprite handle, const sx_vec2 origin) {
    sx_assert(sx_handle_valid(g_spr.sprite_handles, handle.id));
    sprite__data* spr = &g_spr.sprites[sx_handle_index(handle.id)];
    spr->origin = origin;
}

static void sprite__set_color(rizz_sprite handle, const sx_color color) {
    sx_assert(sx_handle_valid(g_spr.sprite_handles, handle.id));
    sprite__data* spr = &g_spr.sprites[sx_handle_index(handle.id)];
    spr->color = color;
}

static void sprite__set_flip(rizz_sprite handle, rizz_sprite_flip flip) {
    sx_assert(sx_handle_valid(g_spr.sprite_handles, handle.id));
    sprite__data* spr = &g_spr.sprites[sx_handle_index(handle.id)];
    spr->flip = flip;
}

// draw-data
static rizz_sprite_drawdata* sprite__drawdata_make_batch(const rizz_sprite* sprs, int num_sprites,
                                                         const sx_alloc* alloc) {
    sx_assert(num_sprites > 0);
    sx_assert(sprs);

    uint64_t* keys = alloca(sizeof(uint64_t) * num_sprites);
    sx_assert(keys);

    // count final vertices and indices,
    int num_verts = 0;
    int num_indices = 0;
    for (int i = 0; i < num_sprites; i++) {
        sx_assert(sprs[i].id);
        int                 index = sx_handle_index(sprs[i].id);
        const sprite__data* spr = &g_spr.sprites[index];

        keys[i] = ((uint64_t)spr->texture.id << 32) | (uint64_t)index;

        if (spr->atlas.id && spr->atlas_sprite_id >= 0) {
            const atlas__data*   atlas = (atlas__data*)the_asset->obj(spr->atlas).ptr;
            const atlas__sprite* aspr = &atlas->sprites[spr->atlas_sprite_id];
            num_verts += aspr->num_verts;
            num_indices += aspr->num_indices;
        } else {
            num_verts += 4;
            num_indices += 6;
        }
    }

    // sort sprites:
    //      high-bits (32): texture handle. main batching
    //      low-bits  (32): sprite index. cache coherence
    if (num_sprites > 1)
        sprite__sort_tim_sort(keys, num_sprites);

    // assume that every sprite is a batch, so we can pre-allocate loosely
    int total_sz = sizeof(rizz_sprite_drawdata) + num_verts * sizeof(rizz_sprite_vertex) +
                   num_indices * sizeof(uint16_t) + sizeof(rizz_sprite_drawbatch) * num_sprites;
    rizz_sprite_drawdata* dd = sx_malloc(alloc, total_sz);
    if (!dd) {
        sx_out_of_memory();
        return NULL;
    }
    memset(dd, 0x0, sizeof(rizz_sprite_drawdata));
    uint8_t* buff = (uint8_t*)(dd + 1);
    dd->batches = (rizz_sprite_drawbatch*)buff;
    buff += sizeof(rizz_sprite_drawbatch) * num_sprites;
    dd->verts = (rizz_sprite_vertex*)buff;
    buff += sizeof(rizz_sprite_vertex) * num_verts;
    dd->indices = (uint16_t*)buff;
    dd->num_batches = 0;
    dd->num_verts = num_verts;
    dd->num_indices = num_indices;

    // fill buffers and batch
    rizz_sprite_vertex* verts = dd->verts;
    uint16_t*           indices = dd->indices;
    int                 index_idx = 0;
    int                 vertex_idx = 0;
    uint32_t            last_batch_key = 0;
    int                 num_batches = 0;
    for (int i = 0; i < num_sprites; i++) {
        int                 index = (int)(keys[i] & 0xffffffff);
        const sprite__data* spr = &g_spr.sprites[index];
        sx_color            color = spr->color;
        int                 index_start = index_idx;
        // atlas
        if (spr->atlas.id && spr->atlas_sprite_id >= 0) {
            // extract sprite rectangle and uv from atlas
            const atlas__data*   atlas = (atlas__data*)the_asset->obj(spr->atlas).ptr;
            const atlas__sprite* aspr = &atlas->sprites[spr->atlas_sprite_id];
            sx_vec2              size = sprite__calc_size(spr->size, aspr->base_size, spr->flip);
            sx_vec2              origin = spr->origin;

            const rizz_sprite_vertex* src_verts = atlas->vertices;
            const uint16_t*           src_indices = atlas->indices;
            rizz_sprite_vertex*       dst_verts = &verts[vertex_idx];
            uint16_t*                 dst_indices = &indices[index_idx];

            for (int i = 0, c = aspr->num_verts; i < c; i++) {
                dst_verts[i].pos = sx_vec2_mul(sx_vec2_sub(src_verts[i].pos, origin), size);
                dst_verts[i].uv = src_verts[i].uv;
                dst_verts[i].color = color;
            }

            for (int i = 0, c = aspr->num_indices; i < c; i += 3) {
                dst_indices[i] = src_indices[i];
                dst_indices[i + 1] = src_indices[i + 1];
                dst_indices[i + 2] = src_indices[i + 2];
            }

            vertex_idx += aspr->num_verts;
            index_idx += aspr->num_indices;
        } else {
            // normal texture sprite: there is no atalas. sprite takes the whole texture
            rizz_texture* tex = (rizz_texture*)the_asset->obj(spr->texture).ptr;
            sx_assert(tex);
            sx_vec2 base_size = sx_vec2f((float)tex->info.width, (float)tex->info.height);
            sx_vec2 size = sprite__calc_size(spr->size, base_size, spr->flip);
            sx_vec2 origin = spr->origin;
            sx_rect rect = sx_rectf(-0.5f, -0.5f, 0.5f, 0.5f);

            verts[0].pos = sx_vec2_mul(sx_vec2_sub(sx_rect_corner(&rect, 0), origin), size);
            verts[0].uv = sx_vec2f(0.0f, 1.0f);
            verts[0].color = color;
            verts[1].pos = sx_vec2_mul(sx_vec2_sub(sx_rect_corner(&rect, 1), origin), size);
            verts[1].uv = sx_vec2f(1.0f, 1.0f);
            verts[2].color = color;
            verts[2].pos = sx_vec2_mul(sx_vec2_sub(sx_rect_corner(&rect, 2), origin), size);
            verts[2].uv = sx_vec2f(0.0f, 0.0f);
            verts[2].color = color;
            verts[3].pos = sx_vec2_mul(sx_vec2_sub(sx_rect_corner(&rect, 3), origin), size);
            verts[3].uv = sx_vec2f(1.0f, 0.0f);
            verts[3].color = color;

            // clang-format off
            indices[0] = 0;         indices[1] = 3;     indices[2] = 1;
            indices[3] = 0;         indices[4] = 2;     indices[5] = 3;
            // clang-format on            

            vertex_idx += 4;
            index_idx += 6;
        }

        // batch by texture
        uint32_t key = spr->texture.id;
        if (last_batch_key != key) {
            rizz_sprite_drawbatch* batch = &dd->batches[num_batches++];
            batch->texture = spr->texture;
            batch->index_start = index_start;
            batch->index_count = index_idx - index_start;
        }
    }

    dd->num_indices = num_indices;
    dd->num_verts = num_verts;
    dd->num_batches = num_batches;
    return dd;
}

static rizz_sprite_drawdata* sprite__drawdata_make(rizz_sprite spr, const sx_alloc* alloc) {
    return sprite__drawdata_make_batch(&spr, 1, alloc);
}

static void sprite__drawdata_free(rizz_sprite_drawdata* data, const sx_alloc* alloc) {
    sx_free(alloc, data);
}

static rizz_api_sprite the__sprite = {
    .sprite_create = sprite__create,
    .sprite_destroy = sprite__destroy,
    .sprite_size = sprite__size,
    .sprite_origin = sprite__origin,
    .sprite_bounds = sprite__bounds,
    .sprite_flip = sprite__flip,
    .sprite_set_size = sprite__set_size,
    .sprite_set_origin = sprite__set_origin,
    .sprite_set_color = sprite__set_color,
    .sprite_set_flip = sprite__set_flip,
    .sprite_drawdata_make = sprite__drawdata_make,
    .sprite_drawdata_make_batch = sprite__drawdata_make_batch,
    .sprite_drawdata_free = sprite__drawdata_free
};

rizz_plugin_decl_main(sprite, plugin, e) {
    switch (e) {
    case RIZZ_PLUGIN_EVENT_STEP:
        break;

    case RIZZ_PLUGIN_EVENT_INIT:
        the_plugin = plugin->api;
        the_core = the_plugin->get_api(RIZZ_API_CORE, 0);
        the_asset = the_plugin->get_api(RIZZ_API_ASSET, 0);
        the_refl = the_plugin->get_api(RIZZ_API_REFLECT, 0);
        if (!sprite__init()) {
            return -1;
        }
        the_plugin->inject_api(RIZZ_API_SPRITE, 0, &the__sprite);
        break;

    case RIZZ_PLUGIN_EVENT_LOAD:
        break;

    case RIZZ_PLUGIN_EVENT_UNLOAD:
        break;

    case RIZZ_PLUGIN_EVENT_SHUTDOWN:
        sprite__release();
        break;
    }

    return 0;
}

rizz_plugin_decl_event_handler(sprite, e) {
    sx_unused(e);
}

rizz_plugin_implement_info(sprite, 1000, "sprite plugin", 0);
