#include "cj5/cj5.h"
#include "rizz/rizz.h"
#include "rizz/json.h"

#include "internal.h"

#include "sx/allocator.h"
#include "sx/io.h"
#include "sx/threads.h"

#define rizz__json_lock()                                  \
    if (params->flags & RIZZ_ASSET_LOAD_FLAG_WAIT_ON_LOAD) \
        sx_lock(&g_json.lock);

#define rizz__json_unlock()                                \
    if (params->flags & RIZZ_ASSET_LOAD_FLAG_WAIT_ON_LOAD) \
        sx_unlock(&g_json.lock);

typedef struct rizz__json_context {
    const sx_alloc* alloc;
    sx_lock_t lock;
} rizz__json_context;

typedef struct rizz__json {
    cj5_result result;
    sx_mem_block* source_mem;
    rizz_json_reload_cb* reload_fn;
    void* user;
} rizz__json;

static rizz__json_context g_json;

// register "json" asset type
// every "json" asset can be associated with a load/reload callback function

static rizz_asset_load_data rizz__json_on_prepare(const rizz_asset_load_params* params, const sx_mem_block* mem)
{
    sx_unused(mem);
    
    const sx_alloc* alloc = params->alloc ? params->alloc : g_json.alloc;

    rizz__json* json = (rizz__json*)sx_malloc(alloc, sizeof(rizz__json));
    if (!json) {
        sx_out_of_memory();
        return (rizz_asset_load_data) { {0} };
    }
    sx_memset(json, 0x0, sizeof(rizz__json));

    return (rizz_asset_load_data) { .obj = {.ptr = json} };
}

static bool rizz__json_on_load(rizz_asset_load_data* data, const rizz_asset_load_params* params,
                               const sx_mem_block* mem)
{
    const sx_alloc* alloc = params->alloc ? params->alloc : g_json.alloc;
    const rizz_json_load_params* jparams = (const rizz_json_load_params*)params->params;
    rizz__json* json = (rizz__json*)data->obj.ptr;
    
    const int num_tmp_tokens = 10000;

    const sx_alloc* tmp_alloc = the__core.tmp_alloc_push();
    
    cj5_token* tmp_tokens = sx_malloc(tmp_alloc, sizeof(cj5_token)*num_tmp_tokens);
    sx_assert_rel(tmp_tokens);

    cj5_result jres = cj5_parse((const char*)mem->data, (int)mem->size, tmp_tokens, num_tmp_tokens);
    if (jres.error) {
        if (jres.error == CJ5_ERROR_OVERFLOW) {
            sx_assert(jres.num_tokens > num_tmp_tokens);
            tmp_tokens = (cj5_token*)sx_realloc(tmp_alloc, tmp_tokens, sizeof(cj5_token)*jres.num_tokens);
            sx_assert_rel(tmp_tokens);
            jres = cj5_parse((const char*)mem->data, (int)mem->size, tmp_tokens, jres.num_tokens);
            if (jres.error) {
                return false;
            }
        } else {
            return false;
        }
    }

    // allocate permanently (lock in async loading)
    rizz__json_lock();
    cj5_token* tokens = sx_malloc(alloc, sizeof(cj5_token)*jres.num_tokens);
    rizz__json_unlock();
    if (!tokens) {
        sx_out_of_memory();
        return false;
    }
    sx_memcpy(tokens, tmp_tokens, sizeof(cj5_token)*jres.num_tokens);

    json->result = jres;
    json->result.tokens = tokens;
    json->source_mem = (sx_mem_block*)mem;
    json->reload_fn = jparams->reload_fn;
    json->user = jparams->user;
    sx_mem_addref(json->source_mem);

    the__core.tmp_alloc_pop();
    return true;
}

static void rizz__json_on_finalize(rizz_asset_load_data* data, const rizz_asset_load_params* params,
                                   const sx_mem_block* mem)
{
    sx_unused(mem);

    rizz__json* json = (rizz__json*)data->obj.ptr;
    const rizz_json_load_params* jparams = (const rizz_json_load_params*)params->params;

    if (jparams->load_fn) {
        jparams->load_fn(&json->result, json->user);
    }
}

static void rizz__json_on_reload(rizz_asset handle, rizz_asset_obj prev_obj, const sx_alloc* alloc)
{
    sx_unused(alloc);

    rizz__json* json = (rizz__json*)the__asset.obj_unsafe(handle).ptr;
    if (json->reload_fn) {
        rizz__json* prev_json = (rizz__json*)prev_obj.ptr;
        json->reload_fn(&json->result, &prev_json->result, json->user);
    }
}

static void rizz__json_on_release(rizz_asset_obj obj, const sx_alloc* alloc)
{
    rizz__json* json = (rizz__json*)obj.ptr;

    if (!alloc) {
        alloc = g_json.alloc;    
    }

    if (json->result.tokens) {
        sx_free(alloc, (cj5_token*)json->result.tokens);
    }
    
    if (json->source_mem) {
        sx_mem_destroy_block(json->source_mem);
    }
}

void rizz__json_init(void)
{
    g_json.alloc = the__core.alloc(RIZZ_MEMID_CORE);

    // TODO: add dummy json (actually, what is the point of having dummy json? so this maybe not necessary)
    the__asset.register_asset_type("json",
                                   (rizz_asset_callbacks){ .on_prepare = rizz__json_on_prepare,
                                                           .on_load = rizz__json_on_load,
                                                           .on_finalize = rizz__json_on_finalize,
                                                           .on_reload = rizz__json_on_reload,
                                                           .on_release = rizz__json_on_release },
                                   "rizz_json_load_params", sizeof(rizz_json_load_params),
                                   (rizz_asset_obj){ .id = 0 }, (rizz_asset_obj){ .id = 0 }, 0);
}