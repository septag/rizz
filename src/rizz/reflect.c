//
// Copyright 2019 Sepehr Taghdisian (septag@github). All rights reserved.
// License: https://github.com/septag/rizz#license-bsd-2-clause
//

#include "internal.h"

#include "sx/array.h"
#include "sx/hash.h"
#include "sx/string.h"

#include <alloca.h>

#define DEFAULT_REG_SIZE 256

typedef struct rizz__refl_struct {
    char type[32];
    int size;    // size of struct
    int num_fields;
} rizz__refl_struct;

typedef struct rizz__refl_enum {
    char type[32];
    int* name_ids;    // index-to: rizz__reflect_context:regs
} rizz__refl_enum;

typedef struct rizz__refl_data {
    rizz_refl_info r;
    int base_id;
    char type[64];    // keep the raw type name, cuz pointer types have '*' in their type names
    char name[32];
    char base[32];
} rizz__refl_data;

typedef struct rizz__refl_field {
    rizz_refl_info info;
    void* value;    // pointer than contains arbitary field value(s) based on info.type/array/etc.
} rizz__refl_field;

typedef struct rizz_refl_context {
    rizz__refl_struct* structs; // sx_array
    rizz__refl_enum* enums;     // sx_array
    rizz__refl_data* regs;      // sx_array
    const sx_alloc* alloc;
    sx_hashtbl* reg_tbl;        // refl.name --> index(regs)
} rizz_refl_context;

static uint32_t k_builtin_type_hashes[_RIZZ_REFL_VARIANTTYPE_COUNT];

static rizz_refl_context* rizz__refl_create_context(const sx_alloc* alloc)
{
    sx_assert(alloc);

    // initialize hashes
    if (!k_builtin_type_hashes[0]) {
        k_builtin_type_hashes[0] = 0xffffffff;
        k_builtin_type_hashes[RIZZ_REFL_VARIANTTYPE_CSTRING] = sx_hash_fnv32_str("const char*");
        k_builtin_type_hashes[RIZZ_REFL_VARIANTTYPE_CHAR] = sx_hash_fnv32_str("char");
        k_builtin_type_hashes[RIZZ_REFL_VARIANTTYPE_FLOAT] = sx_hash_fnv32_str("float");
        k_builtin_type_hashes[RIZZ_REFL_VARIANTTYPE_DOUBLE] = sx_hash_fnv32_str("double"); 
        k_builtin_type_hashes[RIZZ_REFL_VARIANTTYPE_INT32] = sx_hash_fnv32_str("int");
        k_builtin_type_hashes[RIZZ_REFL_VARIANTTYPE_INT8] = sx_hash_fnv32_str("int8_t"); 
        k_builtin_type_hashes[RIZZ_REFL_VARIANTTYPE_INT16] = sx_hash_fnv32_str("int16_t"); 
        k_builtin_type_hashes[RIZZ_REFL_VARIANTTYPE_INT64] = sx_hash_fnv32_str("int64_t");
        k_builtin_type_hashes[RIZZ_REFL_VARIANTTYPE_UINT8] = sx_hash_fnv32_str("uint8_t"); 
        k_builtin_type_hashes[RIZZ_REFL_VARIANTTYPE_UINT16] = sx_hash_fnv32_str("uint16_t"); 
        k_builtin_type_hashes[RIZZ_REFL_VARIANTTYPE_UINT32] = sx_hash_fnv32_str("uint32_t");
        k_builtin_type_hashes[RIZZ_REFL_VARIANTTYPE_UINT64] = sx_hash_fnv32_str("uint64_t"); 
        k_builtin_type_hashes[RIZZ_REFL_VARIANTTYPE_BOOL] = sx_hash_fnv32_str("bool");
        k_builtin_type_hashes[RIZZ_REFL_VARIANTTYPE_MAT4] = sx_hash_fnv32_str("sx_mat4"); 
        k_builtin_type_hashes[RIZZ_REFL_VARIANTTYPE_MAT3] = sx_hash_fnv32_str("sx_mat3"); 
        k_builtin_type_hashes[RIZZ_REFL_VARIANTTYPE_VEC4] = sx_hash_fnv32_str("sx_vec4");
        k_builtin_type_hashes[RIZZ_REFL_VARIANTTYPE_VEC3] = sx_hash_fnv32_str("sx_vec3"); 
        k_builtin_type_hashes[RIZZ_REFL_VARIANTTYPE_VEC2] = sx_hash_fnv32_str("sx_vec2");
        k_builtin_type_hashes[RIZZ_REFL_VARIANTTYPE_IVEC2] = sx_hash_fnv32_str("sx_ivec2");
        k_builtin_type_hashes[RIZZ_REFL_VARIANTTYPE_COLOR] = sx_hash_fnv32_str("sx_color");
        k_builtin_type_hashes[RIZZ_REFL_VARIANTTYPE_AABB] = sx_hash_fnv32_str("sx_aabb");
        k_builtin_type_hashes[RIZZ_REFL_VARIANTTYPE_RECT] = sx_hash_fnv32_str("sx_rect");
    }

    rizz_refl_context* ctx = sx_malloc(alloc, sizeof(rizz_refl_context));
    if (!ctx) {
        sx_memory_fail();
        return NULL;
    }
    sx_memset(ctx, 0x0, sizeof(rizz_refl_context));

    sx_array_reserve(alloc, ctx->regs, DEFAULT_REG_SIZE);

    ctx->reg_tbl = sx_hashtbl_create(alloc, DEFAULT_REG_SIZE);
    if (!ctx->reg_tbl) {
        sx_memory_fail();
        return false;
    }

    ctx->alloc = alloc;

    return ctx;
}

static void rizz__refl_destroy_context(rizz_refl_context* ctx)
{
    sx_assert(ctx);
    sx_assert(ctx->alloc);

    const sx_alloc* alloc = ctx->alloc;
    if (ctx->reg_tbl) {
        sx_hashtbl_destroy(ctx->reg_tbl, alloc);
    }

    for (int i = 0; i < sx_array_count(ctx->enums); i++) {
        sx_array_free(ctx->alloc, ctx->enums[i].name_ids);
    }
    sx_array_free(alloc, ctx->regs);
    sx_array_free(alloc, ctx->structs);
    sx_array_free(alloc, ctx->enums);

    ctx->alloc = NULL;
}

SX_INLINE int rizz__refl_type_size(const char* type_name) {
    if (sx_strequal(type_name, "int") || sx_strequal(type_name, "int32_t"))   return sizeof(int);
    else if (sx_strequal(type_name, "float"))       return sizeof(float);
    else if (sx_strequal(type_name, "char"))        return sizeof(char);
    else if (sx_strequal(type_name, "double"))      return sizeof(double);
    else if (sx_strequal(type_name, "bool"))        return sizeof(bool);
    else if (sx_strequal(type_name, "uint8_t"))     return sizeof(uint8_t);
    else if (sx_strequal(type_name, "uint32_t"))    return sizeof(uint32_t);
    else if (sx_strequal(type_name, "uint64_t"))    return sizeof(uint64_t);
    else if (sx_strequal(type_name, "uint16_t"))    return sizeof(uint16_t);
    else if (sx_strequal(type_name, "int32_t"))     return sizeof(int32_t);
    else if (sx_strequal(type_name, "int16_t"))     return sizeof(int16_t);
    else if (sx_strequal(type_name, "int8_t"))      return sizeof(int8_t);
    else if (sx_strequal(type_name, "int64_t"))     return sizeof(int64_t);

    else if (sx_strequal(type_name, "sx_mat4"))     return sizeof(sx_mat4);
    else if (sx_strequal(type_name, "sx_mat3"))     return sizeof(sx_mat3);
    else if (sx_strequal(type_name, "sx_vec4"))     return sizeof(sx_vec4);
    else if (sx_strequal(type_name, "sx_vec3"))     return sizeof(sx_vec3);
    else if (sx_strequal(type_name, "sx_vec2"))     return sizeof(sx_vec2);
    else if (sx_strequal(type_name, "sx_ivec2"))    return sizeof(sx_ivec2);
    else if (sx_strequal(type_name, "sx_color"))    return sizeof(sx_color);
    else if (sx_strequal(type_name, "sx_aabb"))     return sizeof(sx_aabb);
    else if (sx_strequal(type_name, "sx_rect"))     return sizeof(sx_rect);
    else                                            return 0;
}

SX_INLINE void rizz__refl_set_builtin_type(rizz_refl_variant* var, const void* data, int size)
{
    sx_unused(size);
    switch (var->type) {
    case RIZZ_REFL_VARIANTTYPE_CSTRING: var->str = (const char*)data;    break;
    case RIZZ_REFL_VARIANTTYPE_CHAR:    sx_assert(size == sizeof(char));      var->i8 = *((char*)data);    break;
    case RIZZ_REFL_VARIANTTYPE_FLOAT:   sx_assert(size == sizeof(float));     var->f = *((const float*)data);    break;
    case RIZZ_REFL_VARIANTTYPE_DOUBLE:  sx_assert(size == sizeof(double));    var->d = *((const double*)data);    break;
    case RIZZ_REFL_VARIANTTYPE_INT32:   sx_assert(size == sizeof(int32_t));   var->i32 = *((const int32_t*)data);    break;
    case RIZZ_REFL_VARIANTTYPE_INT8:    sx_assert(size == sizeof(int8_t));    var->i8 = *((const int8_t*)data);    break;
    case RIZZ_REFL_VARIANTTYPE_INT16:   sx_assert(size == sizeof(int16_t));   var->i16 = *((const int16_t*)data);    break;
    case RIZZ_REFL_VARIANTTYPE_INT64:   sx_assert(size == sizeof(int64_t));   var->i64 = *((const int64_t*)data);    break;
    case RIZZ_REFL_VARIANTTYPE_UINT8:   sx_assert(size == sizeof(uint8_t));   var->u8 = *((const uint8_t*)data);    break;
    case RIZZ_REFL_VARIANTTYPE_UINT16:  sx_assert(size == sizeof(uint16_t));  var->u16 = *((const uint16_t*)data);    break;
    case RIZZ_REFL_VARIANTTYPE_UINT32:  sx_assert(size == sizeof(uint32_t));  var->u32 = *((const uint32_t*)data);    break;
    case RIZZ_REFL_VARIANTTYPE_UINT64:  sx_assert(size == sizeof(uint64_t));  var->u64 = *((const uint64_t*)data);    break;
    case RIZZ_REFL_VARIANTTYPE_BOOL:    sx_assert(size == sizeof(bool));      var->b = *((const bool*)data);    break;
    case RIZZ_REFL_VARIANTTYPE_MAT4:    sx_assert(size == sizeof(sx_mat4));   var->mat4 = *((const sx_mat4*)data);    break;
    case RIZZ_REFL_VARIANTTYPE_MAT3:    sx_assert(size == sizeof(sx_mat3));   var->mat3 = *((const sx_mat3*)data);    break;
    case RIZZ_REFL_VARIANTTYPE_VEC4:    sx_assert(size == sizeof(sx_vec4));   var->v4 = *((const sx_vec4*)data);    break;
    case RIZZ_REFL_VARIANTTYPE_VEC3:    sx_assert(size == sizeof(sx_vec3));   var->v3 = *((const sx_vec3*)data);    break;
    case RIZZ_REFL_VARIANTTYPE_VEC2:    sx_assert(size == sizeof(sx_vec2));   var->v2 = *((const sx_vec2*)data);    break;
    case RIZZ_REFL_VARIANTTYPE_IVEC2:   sx_assert(size == sizeof(sx_ivec2));  var->iv2 = *((const sx_ivec2*)data);   break;
    case RIZZ_REFL_VARIANTTYPE_COLOR:   sx_assert(size == sizeof(sx_color));  var->color = *((const sx_color*)data);  break;
    case RIZZ_REFL_VARIANTTYPE_AABB:    sx_assert(size == sizeof(sx_aabb));   var->aabb = *((const sx_aabb*)data);    break;
    case RIZZ_REFL_VARIANTTYPE_RECT:    sx_assert(size == sizeof(sx_rect));   var->rect = *((const sx_rect*)data);    break;
    default: sx_assertf(0, "variant type not supported");
    }

}

SX_INLINE rizz_refl_variant_type rizz__refl_builtin_type(const char* type_name) 
{
    uint32_t type_hash = sx_hash_fnv32_str(type_name);
    for (int i = 0; i < _RIZZ_REFL_VARIANTTYPE_COUNT; i++) {
        if (type_hash == k_builtin_type_hashes[i]) {
            return (rizz_refl_variant_type)i;
        }
    }
    return RIZZ_REFL_VARIANTTYPE_UNKNOWN;
}

static void* rizz__refl_get_func(rizz_refl_context* ctx, const char* name)
{
    int index = sx_hashtbl_find_get(ctx->reg_tbl, sx_hash_fnv32_str(name), -1);
    return (index != -1) ? (ctx->regs[index].r.any) : NULL;
}

static int rizz__refl_get_enum(rizz_refl_context* ctx, const char* name, int not_found)
{
    int index = sx_hashtbl_find_get(ctx->reg_tbl, sx_hash_fnv32_str(name), -1);
    return (index != -1) ? (int)ctx->regs[index].r.offset : not_found;
}

static const char* rizz__refl_get_enum_name(rizz_refl_context* ctx, const char* type, int val)
{
    for (int i = 0, c = sx_array_count(ctx->enums); i < c; i++) {
        if (sx_strequal(ctx->enums[i].type, type)) {
            int* name_ids = ctx->enums[i].name_ids;
            for (int k = 0, kc = sx_array_count(name_ids); k < kc; k++) {
                const rizz__refl_data* r = &ctx->regs[name_ids[k]];
                sx_assert(r->r.internal_type == RIZZ_REFL_ENUM);
                if (val == (int)r->r.offset)
                    return r->name;
            }
        }
    }
    return "";
}

static void* rizz__refl_get_field(rizz_refl_context* ctx, const char* base_type, void* obj, const char* name)
{
    int len = sx_strlen(name) + sx_strlen(base_type) + 2;
    char* base_name = (char*)alloca(len);
    sx_assert(base_name);
    sx_snprintf(base_name, len, "%s.%s", base_type, name);
    int index = sx_hashtbl_find_get(ctx->reg_tbl, sx_hash_fnv32(name, (size_t)len - 1), -1);
    return (index != -1) ? ((uint8_t*)obj + ctx->regs[index].r.offset) : NULL;
}

static int rizz__refl_reg(rizz_refl_context* ctx, rizz_refl_type internal_type, void* any, const char* type,
                          const char* name, const char* base, const char* desc, int size,
                          int base_size, const void* meta)
{
    int id = sx_array_count(ctx->regs);
    int ptr_str_end = 0;

    // for field types, we construct the name by "base.name"
    const char* key;
    if (internal_type == RIZZ_REFL_FIELD) {
        int len = sx_strlen(name) + sx_strlen(base) + 2;
        char* base_name = (char*)alloca(len);
        sx_assert(base_name);
        sx_snprintf(base_name, len, "%s.%s", base, name);
        key = base_name;
    } else {
        if (internal_type == RIZZ_REFL_ENUM) {
            // add enum entry (if doesn't exist)
            int found_idx = -1;
            for (int i = 0, c = sx_array_count(ctx->enums); i < c; i++) {
                if (sx_strequal(ctx->enums[i].type, type)) {
                    found_idx = i;
                    break;
                }
            }

            if (found_idx != -1) {
                rizz__refl_enum* _enum = &ctx->enums[found_idx];
                sx_array_push(ctx->alloc, _enum->name_ids, id);
            } else {
                rizz__refl_enum _enum = (rizz__refl_enum){ .name_ids = NULL };
                sx_strcpy(_enum.type, sizeof(_enum.type), type);
                sx_array_push(ctx->alloc, _enum.name_ids, id);
                sx_array_push(ctx->alloc, ctx->enums, _enum);
            }
        }    // RIZZ_REFL_ENUM

        key = name;
    }

    // registery must not exist
    if (sx_hashtbl_find(ctx->reg_tbl, sx_hash_fnv32_str(key)) >= 0) {
        rizz__log_warn("'%s' is already registered for reflection", key);
        return -1;
    }

    rizz__refl_data r = { .r =
        (rizz_refl_info) {
            .any = any,
            .desc = desc,
            .size = size,
            .array_size = 1,
            .stride = size,
            .internal_type = internal_type,
            .meta = meta
        },
        .base_id = -1 };
    sx_strcpy(r.name, sizeof(r.name), name);
    if (base) {
        sx_strcpy(r.base, sizeof(r.base), base);
    }

    // check for array types []
    const char* bracket = sx_strchar(type, '[');
    if (bracket) {
        // remove whitespace
        while (bracket != type && sx_isspace(*(--bracket)));
        r.r.flags |= RIZZ_REFL_FLAG_IS_ARRAY;
        ptr_str_end = (int16_t)(intptr_t)(bracket - type);
    }

    // check for pointer types
    const char* star = sx_strchar(type, '*');
    if (star) {
        // remove whitespace
        while (star != type && sx_isspace(*(--star)))
            ;
        r.r.flags |= RIZZ_REFL_FLAG_IS_PTR;
        ptr_str_end = (int16_t)(intptr_t)(star - type);
    }

    // check if field is a struct (nested structs)
    if (ptr_str_end) {
        sx_strncpy(r.type, sizeof(r.type), type, ptr_str_end + 1);
    } else {
        sx_strcpy(r.type, sizeof(r.type), type);
    }
    r.r.type = r.type;

    for (int i = 0, c = sx_array_count(ctx->structs); i < c; i++) {
        if (sx_strequal(ctx->structs[i].type, r.type)) {
            r.r.flags |= RIZZ_REFL_FLAG_IS_STRUCT;
            if (r.r.flags & RIZZ_REFL_FLAG_IS_ARRAY) {
                r.r.array_size = size / ctx->structs[i].size;
                r.r.stride = ctx->structs[i].size;
            }
            break;
        }
    }

    // determine size of array elements (built-in types)
    int stride = rizz__refl_type_size(r.type);
    if ((r.r.flags & RIZZ_REFL_FLAG_IS_ARRAY) && !(r.r.flags & RIZZ_REFL_FLAG_IS_STRUCT)) {
        sx_assertf(stride > 0, "invalid built-in type for array");
        r.r.array_size = size / stride;
        r.r.stride = stride;
    }

    if (!stride && !(r.r.flags & RIZZ_REFL_FLAG_IS_STRUCT)) {
        // it's probably an enum
        for (int i = 0, c = sx_array_count(ctx->enums); i < c; i++) {
            if (sx_strequal(ctx->enums[i].type, type)) {
                r.r.flags |= RIZZ_REFL_FLAG_IS_ENUM;
                break;
            }
        }
    }

    // check for base type (struct) and assign the base_id
    if (base) {
        int base_id = -1;
        // search to see if we already have the base type
        for (int i = 0, c = sx_array_count(ctx->structs); i < c; i++) {
            rizz__refl_struct* s = &ctx->structs[i];
            if (sx_strequal(s->type, base)) {
                base_id = i;
                break;
            }
        }

        if (base_id == -1) {
            rizz__refl_struct _base = (rizz__refl_struct){ .size = base_size };
            sx_strcpy(_base.type, sizeof(_base.type), base);
            sx_array_push(ctx->alloc, ctx->structs, _base);
            base_id = sx_array_count(ctx->structs) - 1;
        }

        r.base_id = base_id;
        ++ctx->structs[base_id].num_fields;
    }

    sx_array_push(ctx->alloc, ctx->regs, r);
    sx_hashtbl_add_and_grow(ctx->reg_tbl, sx_hash_fnv32_str(key), id, ctx->alloc);

    return id;
}

static int rizz__refl_size_of(rizz_refl_context* ctx, const char* base_type)
{
    for (int i = 0, c = sx_array_count(ctx->structs); i < c; i++) {
        if (sx_strequal(ctx->structs[i].type, base_type)) {
            return ctx->structs[i].size;
        }
    }

    return 0;
}

static bool rizz__refl_enumerate(rizz_refl_context* ctx, const char* type_name, const void* data,
                                 void* user, const rizz_refl_enumerate_callbacks* callbacks)
{
    sx_assert(callbacks);
    sx_assert(ctx);
    sx_assert(type_name);
    sx_assert(data);

    bool found = false;

    // enumerate struct fields fields
    // search in all registers that base_type (struct) matches type_name
    for (int i = 0, c = sx_array_count(ctx->regs); i < c; i++) {
        rizz__refl_data* r = &ctx->regs[i];
        int num_fields = 0;
        if (r->r.internal_type == RIZZ_REFL_FIELD && sx_strequal(r->base, type_name)) {
            sx_assert(r->base_id < sx_array_count(ctx->structs));
            sx_assert(ctx->structs);
            rizz__refl_struct* s = &ctx->structs[r->base_id];

            bool value_nil = data == NULL;
            void* value = (uint8_t*)data + r->r.offset;
            rizz_refl_info rinfo = { .any = r->r.any,
                                     .type = r->type,
                                     .name = r->name,
                                     .base = r->base,
                                     .desc = r->r.desc,
                                     .size = r->r.size,
                                     .array_size = r->r.array_size,
                                     .stride = r->r.stride,
                                     .flags = r->r.flags,
                                     .internal_type = r->r.internal_type,
                                     .meta = r->r.meta };

            if (r->r.flags & (RIZZ_REFL_FLAG_IS_PTR | RIZZ_REFL_FLAG_IS_ARRAY)) {
                // type names for pointers must only include the _type_ part without '*' or '[]'
                if (value && !value_nil && (r->r.flags & RIZZ_REFL_FLAG_IS_PTR)) {
                    value = (void*)*((uintptr_t*)value);
                }
            }
            
            if (rinfo.flags & RIZZ_REFL_FLAG_IS_ENUM) {
                int e = *((int*)value);
                callbacks->on_enum(rinfo.name, e, rizz__refl_get_enum_name(ctx, rinfo.type, e), user, rinfo.meta);
            }
            else if (rinfo.flags & RIZZ_REFL_FLAG_IS_STRUCT) {
                callbacks->on_struct(rinfo.name, 
                                     rinfo.type, 
                                     (rinfo.flags & RIZZ_REFL_FLAG_IS_ARRAY) ? rinfo.stride : rinfo.size, 
                                     rinfo.array_size,
                                     user, 
                                     rinfo.meta);

                if (rinfo.flags & RIZZ_REFL_FLAG_IS_ARRAY) {
                    for (int i = 0; i < rinfo.array_size; i++) {
                        callbacks->on_struct_array_element(i);
                        rizz__refl_enumerate(ctx, rinfo.type, (uint8_t*)value + (size_t)i*(size_t)rinfo.stride, user, callbacks);
                    }
                } else {
                    if (!rizz__refl_enumerate(ctx, rinfo.type, value, user, callbacks)) {
                        rizz__log_warn("reflection info for '%s' not found", type_name);
                        sx_assertf(0, "reflection info for '%s' not found", type_name);
                        return false;
                    }
                }
            }
            else {
                // built-in types
                rizz_refl_variant_type var_type = rizz__refl_builtin_type(rinfo.type);
                if (rinfo.flags & RIZZ_REFL_FLAG_IS_ARRAY) {

                    if (var_type != RIZZ_REFL_VARIANTTYPE_CHAR) {
                        const sx_alloc* tmp_alloc = the__core.tmp_alloc_push();
                        rizz_refl_variant* vars = sx_malloc(tmp_alloc, rinfo.array_size*sizeof(rizz_refl_variant));
                        sx_assert_always(vars);
                        sx_memset(vars, 0x0,  rinfo.array_size*sizeof(rizz_refl_variant));

                        uint8_t* buff = value;
                        for (int k = 0; k < rinfo.array_size; k++) {
                            vars[k].type = var_type;
                            rizz__refl_set_builtin_type(&vars[k], buff + (size_t)rinfo.stride*(size_t)k, rinfo.stride);
                        }

                        callbacks->on_builtin_array(rinfo.name, vars, rinfo.array_size, user, rinfo.meta);
                        the__core.tmp_alloc_pop();
                    } else {
                        rizz_refl_variant var = { .type = RIZZ_REFL_VARIANTTYPE_CSTRING, .str = (const char*)data };
                        callbacks->on_builtin(rinfo.name, var, user, rinfo.meta);
                    }
                } else {
                    rizz_refl_variant var;
                    var.type = var_type;
                    rizz__refl_set_builtin_type(&var, data, rinfo.size);
                    callbacks->on_builtin(rinfo.name, var, user, rinfo.meta);
                }
            }

            found = true;

            if (++num_fields == s->num_fields) {
                break;    
            }
        }
    }

    sx_assertf(found, "reflection info for '%s' not found", type_name);
    return found;
}

static int rizz__refl_reg_count(rizz_refl_context* ctx)
{
    return sx_array_count(ctx->regs);
}

rizz_api_refl the__refl = { .create_context = rizz__refl_create_context,
                            .destroy_context = rizz__refl_destroy_context,
                            ._reg_private = rizz__refl_reg,
                            .size_of = rizz__refl_size_of,
                            .get_func = rizz__refl_get_func,
                            .get_enum = rizz__refl_get_enum,
                            .get_field = rizz__refl_get_field,
                            .get_enum_name = rizz__refl_get_enum_name,
                            .reg_count = rizz__refl_reg_count,
                            .enumerate = rizz__refl_enumerate };
