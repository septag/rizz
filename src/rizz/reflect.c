//
// Copyright 2019 Sepehr Taghdisian (septag@github). All rights reserved.
// License: https://github.com/septag/rizz#license-bsd-2-clause
//

#include "sx/allocator.h"
#include "sx/array.h"
#include "sx/hash.h"
#include "sx/string.h"
#include "sx/io.h"

#define CJ5_IGNORE_IMPLEMENT
#include "rizz/json.h"  // for json serialization
#undef CJ5_IGNORE_IMPLEMENT

#include "internal.h"

#include <alloca.h>

#define DEFAULT_REG_SIZE 256

typedef struct refl__struct {
    char type[64];
    int size;    // size of struct
    int num_fields;
} refl__struct;

typedef struct refl__enum {
    char type[64];
    int* name_ids;    // index-to: rizz__reflect_context:regs
} refl__enum;

typedef struct refl__data {
    rizz_refl_info r;
    int base_id;
    char type[64];    // keep the raw type name, cuz pointer types have '*' in their type names
    char name[64];
    char base[64];
    uint32_t base_hash;
} refl__data;

typedef struct rizz_refl_context {
    refl__struct* structs; // sx_array
    refl__enum* enums;     // sx_array
    refl__data* regs;      // sx_array
    const sx_alloc* alloc;
    sx_hashtbl* reg_tbl;        // refl.name --> index(regs)
} rizz_refl_context;

static uint32_t k_builtin_type_hashes[_RIZZ_REFL_VARIANTTYPE_COUNT];

static rizz_refl_context* refl__create_context(const sx_alloc* alloc)
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

static void refl__destroy_context(rizz_refl_context* ctx)
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

SX_INLINE int refl__type_size(const char* type_name) {
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

SX_INLINE void refl__set_builtin_type(rizz_refl_variant* var, const void* data, int size)
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

SX_INLINE rizz_refl_variant_type refl__builtin_type(const char* type_name) 
{
    uint32_t type_hash = sx_hash_fnv32_str(type_name);
    for (int i = 0; i < _RIZZ_REFL_VARIANTTYPE_COUNT; i++) {
        if (type_hash == k_builtin_type_hashes[i]) {
            return (rizz_refl_variant_type)i;
        }
    }
    return RIZZ_REFL_VARIANTTYPE_UNKNOWN;
}

static void* refl__get_func(rizz_refl_context* ctx, const char* name)
{
    int index = sx_hashtbl_find_get(ctx->reg_tbl, sx_hash_fnv32_str(name), -1);
    return (index != -1) ? (ctx->regs[index].r.any) : NULL;
}

static int refl__get_enum(rizz_refl_context* ctx, const char* name, int not_found)
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
                const refl__data* r = &ctx->regs[name_ids[k]];
                sx_assert(r->r.internal_type == RIZZ_REFL_ENUM);
                if (val == (int)r->r.offset)
                    return r->name;
            }
        }
    }
    return "";
}

static void* refl__get_field(rizz_refl_context* ctx, const char* base_type, void* obj, const char* name)
{
    int len = sx_strlen(name) + sx_strlen(base_type) + 2;
    char* base_name = (char*)alloca(len);
    sx_assert(base_name);
    sx_snprintf(base_name, len, "%s.%s", base_type, name);
    int index = sx_hashtbl_find_get(ctx->reg_tbl, sx_hash_fnv32(name, (size_t)len - 1), -1);
    return (index != -1) ? ((uint8_t*)obj + ctx->regs[index].r.offset) : NULL;
}

static int refl__reg(rizz_refl_context* ctx, rizz_refl_type internal_type, void* any, const char* type,
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
                refl__enum* _enum = &ctx->enums[found_idx];
                sx_array_push(ctx->alloc, _enum->name_ids, id);
            } else {
                refl__enum _enum = (refl__enum){ .name_ids = NULL };
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

    refl__data r = { .r =
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
        r.base_hash = sx_hash_fnv32_str(base);
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
    int stride = refl__type_size(r.type);
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
            refl__struct* s = &ctx->structs[i];
            if (sx_strequal(s->type, base)) {
                base_id = i;
                break;
            }
        }

        if (base_id == -1) {
            refl__struct _base = (refl__struct){ .size = base_size };
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

static int refl__size_of(rizz_refl_context* ctx, const char* base_type)
{
    for (int i = 0, c = sx_array_count(ctx->structs); i < c; i++) {
        if (sx_strequal(ctx->structs[i].type, base_type)) {
            return ctx->structs[i].size;
        }
    }

    return 0;
}

static bool refl__serialize_internal(rizz_refl_context* ctx, const char* type_name, const void* data,
                                     void* user, const rizz_refl_serialize_callbacks* callbacks)
{
    sx_assert(callbacks);
    sx_assert(ctx);
    sx_assert(type_name);
    sx_assert(data);

    bool found = false;
    
    // enumerate struct fields fields
    // search in all registers that base_type (struct) matches type_name
    uint32_t type_hash = sx_hash_fnv32_str(type_name);
    int reg_ids[256];
    int num_reg_ids = 0;
    for (int i = 0, c = sx_array_count(ctx->regs); i < c; i++) {
        refl__data* r = &ctx->regs[i];
        if (r->r.internal_type == RIZZ_REFL_FIELD && type_hash == r->base_hash) {
            reg_ids[num_reg_ids++] = i;
            sx_assert(num_reg_ids <= 256);
        }
    }

    for (int i = 0; i < num_reg_ids; i++) {
        bool last_one = i == (num_reg_ids - 1);
        refl__data* r = &ctx->regs[reg_ids[i]];
        int num_fields = 0;
        if (r->r.internal_type == RIZZ_REFL_FIELD && sx_strequal(r->base, type_name)) {
            sx_assert(r->base_id < sx_array_count(ctx->structs));
            sx_assert(ctx->structs);
            refl__struct* s = &ctx->structs[r->base_id];

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
                callbacks->on_enum(rinfo.name, e, rizz__refl_get_enum_name(ctx, rinfo.type, e), user, rinfo.meta, last_one);
            }
            else if (rinfo.flags & RIZZ_REFL_FLAG_IS_STRUCT) {
                callbacks->on_struct_begin(rinfo.name, 
                                           rinfo.type, 
                                           (rinfo.flags & RIZZ_REFL_FLAG_IS_ARRAY) ? rinfo.stride : rinfo.size, 
                                           rinfo.array_size,
                                           user, 
                                           rinfo.meta);

                if (rinfo.flags & RIZZ_REFL_FLAG_IS_ARRAY) {
                    for (int k = 0; k < rinfo.array_size; k++) {
                        callbacks->on_struct_array_element(k, user, rinfo.meta);
                        refl__serialize_internal(ctx, rinfo.type, (uint8_t*)value + (size_t)k*(size_t)rinfo.stride,
                                                 user, callbacks);
                    }
                } else {
                    if (!refl__serialize_internal(ctx, rinfo.type, value, user, callbacks)) {
                        rizz__log_warn("reflection info for '%s' not found", type_name);
                        sx_assertf(0, "reflection info for '%s' not found", type_name);
                        return false;
                    }
                }

                callbacks->on_struct_end(user, rinfo.meta, last_one);
            }
            else {
                // built-in types
                rizz_refl_variant_type var_type = refl__builtin_type(rinfo.type);
                if (rinfo.flags & RIZZ_REFL_FLAG_IS_ARRAY) {

                    if (var_type != RIZZ_REFL_VARIANTTYPE_CHAR) {
                        rizz__with_temp_alloc(tmp_alloc) {
                            rizz_refl_variant* vars = sx_malloc(tmp_alloc, rinfo.array_size*sizeof(rizz_refl_variant));
                            sx_assert_always(vars);
                            sx_memset(vars, 0x0,  rinfo.array_size*sizeof(rizz_refl_variant));

                            uint8_t* buff = value;
                            for (int k = 0; k < rinfo.array_size; k++) {
                                vars[k].type = var_type;
                                refl__set_builtin_type(&vars[k], buff + (size_t)rinfo.stride*(size_t)k, rinfo.stride);
                            }

                            callbacks->on_builtin_array(rinfo.name, vars, rinfo.array_size, user, rinfo.meta, last_one);
                        }
                    } else {
                        rizz_refl_variant var = { .type = RIZZ_REFL_VARIANTTYPE_CSTRING, .str = (const char*)value };
                        callbacks->on_builtin(rinfo.name, var, user, rinfo.meta, last_one);
                    }
                } else {
                    rizz_refl_variant var;
                    var.type = var_type;
                    refl__set_builtin_type(&var, value, rinfo.size);
                    callbacks->on_builtin(rinfo.name, var, user, rinfo.meta, last_one);
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

static bool refl__serialize(rizz_refl_context* ctx, const char* type_name, const void* data,
                            void* user, const rizz_refl_serialize_callbacks* callbacks)
{
    if (!callbacks->on_begin(type_name, user)) {
        return false;
    }

    bool r = refl__serialize_internal(ctx, type_name, data, user, callbacks);
    
    callbacks->on_end(user);
    
    return r;
}

static bool refl__deserialize_internal(rizz_refl_context* ctx, const char* type_name, void* data,
                                       void* user, const rizz_refl_deserialize_callbacks* callbacks)
{
    sx_assert(callbacks);
    sx_assert(ctx);
    sx_assert(type_name);
    sx_assert(data);

    bool found = false;
    
    // enumerate struct fields fields
    // search in all registers that base_type (struct) matches type_name
    uint32_t type_hash = sx_hash_fnv32_str(type_name);
    int reg_ids[256];
    int num_reg_ids = 0;
    for (int i = 0, c = sx_array_count(ctx->regs); i < c; i++) {
        refl__data* r = &ctx->regs[i];
        if (r->r.internal_type == RIZZ_REFL_FIELD && type_hash == r->base_hash) {
            reg_ids[num_reg_ids++] = i;
            sx_assert(num_reg_ids <= 256);
        }
    }

    for (int i = 0; i < num_reg_ids; i++) {
        bool last_one = i == (num_reg_ids - 1);
        refl__data* r = &ctx->regs[reg_ids[i]];
        int num_fields = 0;
        if (r->r.internal_type == RIZZ_REFL_FIELD && sx_strequal(r->base, type_name)) {
            sx_assert(r->base_id < sx_array_count(ctx->structs));
            sx_assert(ctx->structs);
            refl__struct* s = &ctx->structs[r->base_id];

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
                callbacks->on_enum(rinfo.name, (int*)value, user, rinfo.meta, last_one);
            }
            else if (rinfo.flags & RIZZ_REFL_FLAG_IS_STRUCT) {
                callbacks->on_struct_begin(rinfo.name, 
                                           rinfo.type, 
                                           (rinfo.flags & RIZZ_REFL_FLAG_IS_ARRAY) ? rinfo.stride : rinfo.size, 
                                           rinfo.array_size,
                                           user, 
                                           rinfo.meta);

                if (rinfo.flags & RIZZ_REFL_FLAG_IS_ARRAY) {
                    for (int k = 0; k < rinfo.array_size; k++) {
                        callbacks->on_struct_array_element(k, user, rinfo.meta);
                        refl__deserialize_internal(ctx, rinfo.type, (uint8_t*)value + (size_t)k*(size_t)rinfo.stride,
                                                   user, callbacks);
                    }
                } else {
                    if (!refl__deserialize_internal(ctx, rinfo.type, value, user, callbacks)) {
                        rizz__log_warn("reflection info for '%s' not found", type_name);
                        sx_assertf(0, "reflection info for '%s' not found", type_name);
                        return false;
                    }
                }

                callbacks->on_struct_end(user, rinfo.meta, last_one);
            }
            else {
                // built-in types
                rizz_refl_variant_type var_type = refl__builtin_type(rinfo.type);
                if (rinfo.flags & RIZZ_REFL_FLAG_IS_ARRAY) {
                    sx_memset(value, 0x0,  (size_t)rinfo.stride*(size_t)rinfo.array_size);
                    if (var_type != RIZZ_REFL_VARIANTTYPE_CHAR) {
                        callbacks->on_builtin_array(rinfo.name, value, var_type, rinfo.array_size, rinfo.stride, 
                                                    user, rinfo.meta, last_one);
                    } else {
                        callbacks->on_builtin(rinfo.name, value, RIZZ_REFL_VARIANTTYPE_CSTRING, rinfo.array_size,
                                              user, rinfo.meta, last_one);
                    }
                } else {
                    callbacks->on_builtin(rinfo.name, value, var_type, rinfo.size, user, rinfo.meta, last_one);
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


static bool refl__deserialize(rizz_refl_context* ctx, const char* type_name, void* data,
                              void* user, const rizz_refl_deserialize_callbacks* callbacks)
{
    if (!callbacks->on_begin(type_name, user)) {
        return false;
    }

    bool r = refl__deserialize_internal(ctx, type_name, data, user, callbacks);

    callbacks->on_end(user);

    return r;
}

static int refl__get_fields(rizz_refl_context* ctx, const char* base_type, void* obj, 
                            rizz_refl_field* fields, int max_fields)
{
    int num_fields = 0;
    for (int i = 0, c = sx_array_count(ctx->regs); i < c; i++) {
        refl__data* r = &ctx->regs[i];
        if (r->r.internal_type == RIZZ_REFL_FIELD && sx_strequal(r->base, base_type)) {
            sx_assert(r->base_id < sx_array_count(ctx->structs));
            sx_assert(ctx->structs);
            refl__struct* s = &ctx->structs[r->base_id];

            if (fields && num_fields < max_fields) {
                bool value_nil = obj == NULL;
                void* value = (uint8_t*)obj + r->r.offset;
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

                if (!(r->r.flags & (RIZZ_REFL_FLAG_IS_PTR | RIZZ_REFL_FLAG_IS_ARRAY))) {
                    fields[num_fields] = (rizz_refl_field){ .info = rinfo, .value = value };
                } else {
                    // type names for pointers must only include the _type_ part without '*' or '[]'
                    if (value && !value_nil && (r->r.flags & RIZZ_REFL_FLAG_IS_PTR))
                        value = (void*)*((uintptr_t*)value);
                    fields[num_fields] = (rizz_refl_field){ .info = rinfo, .value = value };
                }
            }    // if (fields)

            ++num_fields;
            if (num_fields == s->num_fields)
                break;
        }
    }

    return num_fields;
}

static int refl__reg_count(rizz_refl_context* ctx)
{
    return sx_array_count(ctx->regs);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// json serialization
typedef struct refl__write_json_context {
    const sx_alloc* alloc;
    sx_mem_writer writer;
    const char* newline;
    const char* tab;

    int _depth;
    bool _is_struct_array;
    char _tabs[128];
    int _array_count;
} refl__write_json_context;


#define JSON_STACK_COUNT 32
typedef struct refl__read_json_context {
    rizz_refl_context* rctx;
    rizz_json* json;
    int cur_token;
    int struct_array_parent;
    int token_stack[JSON_STACK_COUNT];
    int token_stack_idx;
} refl__read_json_context;

SX_INLINE void refl__writef(sx_mem_writer* writer, const char* fmt, ...)
{
    char str[1024];
    va_list args;
    va_start(args, fmt);
    sx_vsnprintf(str, sizeof(str), fmt, args);
    va_end(args);
    
    sx_mem_write(writer, str, sx_strlen(str));
}

static const char* refl__serialize_json_update_tabs(refl__write_json_context* jctx)
{
    if (jctx->tab[0]) {
        sx_strcpy(jctx->_tabs, sizeof(jctx->_tabs), jctx->tab);
        for (int i = 0; i < jctx->_depth; i++) {
            sx_strcat(jctx->_tabs, sizeof(jctx->_tabs), jctx->tab);
        }
    } else {
        jctx->_tabs[0] = '\0';
    }

    return jctx->_tabs;
}

static bool refl__serialize_json_begin(const char* type_name, void* user) 
{
    sx_unused(type_name);

    refl__write_json_context* jctx = user;

    sx_mem_init_writer(&jctx->writer, jctx->alloc, 4096);

    if (!jctx->newline) {
        jctx->newline = "";
    }
    if (!jctx->tab) {
        jctx->tab = "";
    }
    
    refl__writef(&jctx->writer, "{%s", jctx->newline);

    refl__serialize_json_update_tabs(jctx);
    return true;
}

static void refl__serialize_json_end(void* user)
{
    refl__write_json_context* jctx = user;

    refl__writef(&jctx->writer, "}%s", jctx->newline);

    // close the string
    char eof = '\0';
    sx_mem_write(&jctx->writer, &eof, sizeof(eof));
}

#define COMMA() (!last_in_parent ? "," : "")

static void refl__serialize_json_builtin(const char* name, rizz_refl_variant value, void* user, 
                                         const void* meta, bool last_in_parent)
{
    sx_unused(meta);

    refl__write_json_context* jctx = user;
    const char* tabs = jctx->_tabs;
    switch (value.type) {
    case RIZZ_REFL_VARIANTTYPE_FLOAT:   
        refl__writef(&jctx->writer, "%s\"%s\": %f%s%s", tabs, name, value.f, COMMA(), jctx->newline);   
        break;
    case RIZZ_REFL_VARIANTTYPE_INT32:   
        refl__writef(&jctx->writer, "%s\"%s\": %d%s%s", tabs, name, value.i32, COMMA(), jctx->newline); 
        break;
    case RIZZ_REFL_VARIANTTYPE_BOOL:    
        refl__writef(&jctx->writer, "%s\"%s\": %s%s%s", tabs, name, value.b ? "true" : "false", COMMA(), jctx->newline);   
        break;
    case RIZZ_REFL_VARIANTTYPE_CSTRING: 
        refl__writef(&jctx->writer, "%s\"%s\": \"%s\"%s%s", tabs, name, value.str, COMMA(), jctx->newline); 
        break;
    case RIZZ_REFL_VARIANTTYPE_VEC3:
        refl__writef(&jctx->writer, "%s\"%s\": [%f, %f, %f]%s%s", tabs, name, value.v3.x, value.v3.y, value.v3.z, 
                COMMA(), jctx->newline); 
        break;
    case RIZZ_REFL_VARIANTTYPE_MAT4:
        refl__writef(&jctx->writer, "%s\"%s\": [%f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f]%s%s", 
                     tabs, name, 
                     value.mat4.f[0], value.mat4.f[1], value.mat4.f[2], value.mat4.f[3],
                     value.mat4.f[4], value.mat4.f[5], value.mat4.f[6], value.mat4.f[7],
                     value.mat4.f[8], value.mat4.f[9], value.mat4.f[10], value.mat4.f[11],
                     value.mat4.f[12], value.mat4.f[13], value.mat4.f[14], value.mat4.f[15],
                     COMMA(), jctx->newline);
        break;
    case RIZZ_REFL_VARIANTTYPE_VEC4:
        refl__writef(&jctx->writer, "%s\"%s\": [%f, %f, %f, %f]%s%s", tabs, name, 
                value.v4.x, value.v4.y, value.v4.z, value.v4.w, COMMA(), jctx->newline);
        break;
    case RIZZ_REFL_VARIANTTYPE_VEC2:
        refl__writef(&jctx->writer, "%s\"%s\": [%f, %f]%s%s", tabs, name, value.v2.x, value.v2.y, COMMA(), jctx->newline);
        break;
    case RIZZ_REFL_VARIANTTYPE_IVEC2:
        refl__writef(&jctx->writer, "%s\"%s\": [%d, %d]%s%s", tabs, name, value.iv2.x, value.iv2.y, COMMA(), jctx->newline);
        break;
    case RIZZ_REFL_VARIANTTYPE_COLOR:
        refl__writef(&jctx->writer, "%s\"%s\": [%d, %d, %d, %d]%s%s", tabs, name, 
                value.color.r, value.color.g, value.color.b, value.color.a, COMMA(), jctx->newline);
        break;
    case RIZZ_REFL_VARIANTTYPE_MAT3:
        refl__writef(&jctx->writer,
                "%s\"%s\": [%f, %f, %f, %f, %f, %f, %f, %f, %f]%s%s",
                tabs, name, 
                value.mat3.f[0], value.mat3.f[1], value.mat3.f[2], 
                value.mat3.f[3], value.mat3.f[4], value.mat3.f[5], 
                value.mat3.f[6], value.mat3.f[7], value.mat3.f[8], 
                COMMA(), jctx->newline);
        break;
    case RIZZ_REFL_VARIANTTYPE_DOUBLE:
        refl__writef(&jctx->writer, "%s\"%s\": %fL%s%s", tabs, name, value.d, COMMA(), jctx->newline);
        break;

    case RIZZ_REFL_VARIANTTYPE_UINT32:
        refl__writef(&jctx->writer, "%s\"%s\": %u%s%s", tabs, name, value.u32, COMMA(), jctx->newline);
        break;
    case RIZZ_REFL_VARIANTTYPE_AABB:
        refl__writef(&jctx->writer, "%s\"%s\": [%f, %f, %f, %f, %f, %f]%s%s", tabs, name, value.aabb.xmin,
                value.aabb.ymin, value.aabb.zmin, value.aabb.xmax, value.aabb.ymax,
                value.aabb.zmax, COMMA(), jctx->newline);
        break;
    case RIZZ_REFL_VARIANTTYPE_RECT:
        refl__writef(&jctx->writer, "%s\"%s\": [%f, %f, %f, %f]%s%s", tabs, name, value.rect.xmin,
                value.rect.ymin, value.rect.xmax, value.aabb.ymax, COMMA(), jctx->newline);
        break;
    case RIZZ_REFL_VARIANTTYPE_INT8:
        refl__writef(&jctx->writer, "%s\"%s\": %d%s%s", tabs, name, value.i8, COMMA(), jctx->newline);
        break;
    case RIZZ_REFL_VARIANTTYPE_INT16:
        refl__writef(&jctx->writer, "%s\"%s\": %d%s%s", tabs, name, value.i16, COMMA(), jctx->newline);
        break;
    case RIZZ_REFL_VARIANTTYPE_INT64:
        refl__writef(&jctx->writer, "%s\"%s\": %lld%s%s", tabs, name, value.i64, COMMA(), jctx->newline);
        break;
    case RIZZ_REFL_VARIANTTYPE_UINT8:
        refl__writef(&jctx->writer, "%s\"%s\": %u%s%s", tabs, name, value.u8, COMMA(), jctx->newline);
        break;
    case RIZZ_REFL_VARIANTTYPE_UINT16:
        refl__writef(&jctx->writer, "%s\"%s\": %u%s%s", tabs, name, value.u16, COMMA(), jctx->newline);
        break;
    case RIZZ_REFL_VARIANTTYPE_UINT64:
        refl__writef(&jctx->writer, "%s\"%s\": %llu%s%s", tabs, name, value.u64, COMMA(), jctx->newline);
        break;
    case RIZZ_REFL_VARIANTTYPE_CHAR:
        refl__writef(&jctx->writer, "%s\"%s\": \"%c\"%s%s", tabs, name, value.i8, COMMA(), jctx->newline);
        break;
    default:  
        sx_assertf(0, "unsupported type");
        break;
    }    
}

static void refl__serialize_json_builtin_array(const char* name, const rizz_refl_variant* vars, int count,
                                               void* user, const void* meta, bool last_in_parent)
{
    sx_unused(meta);
    sx_unused(last_in_parent);

    refl__write_json_context* jctx = user;
    refl__writef(&jctx->writer, "%s\"%s\": [", jctx->_tabs, name);
    for (int i = 0; i < count; i++) {
        const char* comma =  (i < count - 1) ? "," : "";
        rizz_refl_variant value = vars[i];
        switch (vars[i].type) {
        case RIZZ_REFL_VARIANTTYPE_FLOAT:
            refl__writef(&jctx->writer, "%f%s", value.f, comma);
            break;
        case RIZZ_REFL_VARIANTTYPE_INT32:
            refl__writef(&jctx->writer, "%d%s", value.i32, comma);
            break;
        case RIZZ_REFL_VARIANTTYPE_BOOL:
            refl__writef(&jctx->writer, "%s%s", value.b ? "true" : "false", comma);
            break;
        case RIZZ_REFL_VARIANTTYPE_CSTRING:
            refl__writef(&jctx->writer, "\"%s\"%s", vars[i].str, comma);
            break;
        case RIZZ_REFL_VARIANTTYPE_VEC3:
            refl__writef(&jctx->writer, "[%f, %f, %f]%s", value.v3.x, value.v3.y, value.v3.z, comma); 
            break;
        case RIZZ_REFL_VARIANTTYPE_MAT4:
            refl__writef(&jctx->writer, "[%f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f]%s", 
                    value.mat4.f[0], value.mat4.f[1], value.mat4.f[2], value.mat4.f[3],
                    value.mat4.f[4], value.mat4.f[5], value.mat4.f[6], value.mat4.f[7],
                    value.mat4.f[8], value.mat4.f[9], value.mat4.f[10], value.mat4.f[11],
                    value.mat4.f[12], value.mat4.f[13], value.mat4.f[14], value.mat4.f[15],
                    comma);
            break;
        case RIZZ_REFL_VARIANTTYPE_VEC4:
            refl__writef(&jctx->writer, "[%f, %f, %f, %f]%s", 
                    value.v4.x, value.v4.y, value.v4.z, value.v4.w, comma);
            break;
        case RIZZ_REFL_VARIANTTYPE_VEC2:
            refl__writef(&jctx->writer, "[%f, %f]%s", value.v2.x, value.v2.y, comma);
            break;
        case RIZZ_REFL_VARIANTTYPE_IVEC2:
            refl__writef(&jctx->writer, "[%d, %d]%s", value.iv2.x, value.iv2.y, comma);
            break;
        case RIZZ_REFL_VARIANTTYPE_COLOR:
            refl__writef(&jctx->writer, "[%d, %d, %d, %d]%s", 
                    value.color.r, value.color.g, value.color.b, value.color.a, comma);
            break;
        case RIZZ_REFL_VARIANTTYPE_MAT3:
            refl__writef(&jctx->writer, "[%f, %f, %f, %f, %f, %f, %f, %f, %f]%s",                    
                    value.mat3.f[0], value.mat3.f[1], value.mat3.f[2], 
                    value.mat3.f[3], value.mat3.f[4], value.mat3.f[5], 
                    value.mat3.f[6], value.mat3.f[7], value.mat3.f[8], 
                    comma);
            break;
        case RIZZ_REFL_VARIANTTYPE_DOUBLE:
            refl__writef(&jctx->writer, "%fL%s", value.d, comma);
            break;

        case RIZZ_REFL_VARIANTTYPE_UINT32:
            refl__writef(&jctx->writer, "%u%s", value.u32, comma);
            break;
        case RIZZ_REFL_VARIANTTYPE_AABB:
            refl__writef(&jctx->writer, "[%f, %f, %f, %f, %f, %f]%s", value.aabb.xmin,
                    value.aabb.ymin, value.aabb.zmin, value.aabb.xmax, value.aabb.ymax,
                    value.aabb.zmax, comma);
            break;
        case RIZZ_REFL_VARIANTTYPE_RECT:
            refl__writef(&jctx->writer, "[%f, %f, %f, %f]%s", value.rect.xmin,
                    value.rect.ymin, value.rect.xmax, value.aabb.ymax, comma);
            break;
        case RIZZ_REFL_VARIANTTYPE_INT8:
            refl__writef(&jctx->writer, "%d%s", value.i8, comma);
            break;
        case RIZZ_REFL_VARIANTTYPE_INT16:
            refl__writef(&jctx->writer, "%d%s", value.i16, comma);
            break;
        case RIZZ_REFL_VARIANTTYPE_INT64:
            refl__writef(&jctx->writer, "%lld%s", value.i64, comma);
            break;
        case RIZZ_REFL_VARIANTTYPE_UINT8:
            refl__writef(&jctx->writer, "%u%s", value.u8, comma);
            break;
        case RIZZ_REFL_VARIANTTYPE_UINT16:
            refl__writef(&jctx->writer, "%u%s", value.u16, comma);
            break;
        case RIZZ_REFL_VARIANTTYPE_UINT64:
            refl__writef(&jctx->writer, "%llu%s", value.u64, comma);
            break;
        case RIZZ_REFL_VARIANTTYPE_CHAR:
            refl__writef(&jctx->writer, "\"%c\"%s", value.i8, comma);
            break;
        default:
            sx_assertf(0, "unsupported type");
            break;
        }
    }
    refl__writef(&jctx->writer, "]%s%s", COMMA(), jctx->newline);
}

static void refl__serialize_json_struct_begin(const char* name, const char* type_name, int size, 
                                              int count, void* user, const void* meta)
{
    sx_unused(type_name);
    sx_unused(size);
    sx_unused(meta);

    refl__write_json_context* jctx = user;

    if (count == 1) {
        refl__writef(&jctx->writer, "%s\"%s\": {%s", jctx->_tabs, name, jctx->newline);

    } else {
        refl__writef(&jctx->writer, "%s\"%s\": [{%s", jctx->_tabs, name, jctx->newline);
        jctx->_is_struct_array = true;
    }

    ++jctx->_depth;
    refl__serialize_json_update_tabs(jctx);
    
    jctx->_array_count = count;
}

static void refl__serialize_json_struct_array_element(int index, void* user, const void* meta)
{
    sx_unused(meta);

    refl__write_json_context* jctx = user;
    if (index != 0) {
        char tabs[128];
        if (jctx->tab[0]) {
            sx_strncpy(tabs, sizeof(tabs), jctx->_tabs, sx_strlen(jctx->_tabs) - sx_strlen(jctx->tab));
        } else {
            tabs[0] = '\0';
        }
        refl__writef(&jctx->writer, "%s},%s%s{%s", tabs, jctx->newline, tabs, jctx->newline);
    } 
}

static void refl__serialize_json_struct_end(void* user, const void* meta, bool last_in_parent)
{
    sx_unused(meta);
    sx_unused(last_in_parent);

    refl__write_json_context* jctx = user;

    char tabs[128];
    if (jctx->tab[0]) {
        sx_strncpy(tabs, sizeof(tabs), jctx->_tabs, sx_strlen(jctx->_tabs) - sx_strlen(jctx->tab));
    } else {
        tabs[0] = '\0';
    }

    if (jctx->_is_struct_array) {
        refl__writef(&jctx->writer, "%s}]%s%s", tabs, COMMA(), jctx->newline);
        jctx->_is_struct_array = false;
    } else {
        refl__writef(&jctx->writer, "%s}%s%s", tabs, COMMA(), jctx->newline);
    }
    --jctx->_depth;
    refl__serialize_json_update_tabs(jctx);
}

static void refl__serialize_json_enum(const char* name, int value, const char* value_name, void* user,
                                      const void* meta, bool last_in_parent)
{
    sx_unused(value);
    sx_unused(meta);
    sx_unused(last_in_parent);

    refl__write_json_context* jctx = user;

    refl__writef(&jctx->writer, "%s\"%s\": \"%s\"%s%s", jctx->_tabs, name, value_name, COMMA(), jctx->newline);
}

static bool refl__deserialize_json_begin(const char* type_name, void* user)
{
    sx_unused(type_name);

    refl__read_json_context* jctx = user;
    jctx->struct_array_parent = -1;
    return true;
}

static void refl__deserialize_json_end(void* user)
{
    sx_unused(user);
}

static void refl__deserialize_json_builtin(const char* name, void* data, rizz_refl_variant_type type, 
                                           int size, void* user, const void* meta, bool last_in_parent)
{
    sx_unused(meta);
    sx_unused(last_in_parent);

    refl__read_json_context* jctx = user;
    cj5_result* r = &jctx->json->result;

    switch (type) {
    case RIZZ_REFL_VARIANTTYPE_INT32:   
        sx_assert(size == sizeof(int));
        *((int*)data) = cj5_seekget_int(r, jctx->cur_token, name, 0);
        break;
    case RIZZ_REFL_VARIANTTYPE_FLOAT:
        sx_assert(size == sizeof(float));
        *((float*)data) = cj5_seekget_float(r, jctx->cur_token, name, 0);
        break;
    case RIZZ_REFL_VARIANTTYPE_BOOL:
        sx_assert(size == sizeof(bool));
        *((bool*)data) = cj5_seekget_bool(r, jctx->cur_token, name, 0);
        break;
    case RIZZ_REFL_VARIANTTYPE_CSTRING: 
    {
        char* str = alloca(size);
        sx_assert_always(str);
        sx_strcpy(data, size, cj5_seekget_string(r, jctx->cur_token, name, str, size+1, ""));
        break;                          
    }
    case RIZZ_REFL_VARIANTTYPE_VEC3:
        sx_assert(size == sizeof(sx_vec3));
        cj5_seekget_array_float(r, jctx->cur_token, name, ((sx_vec3*)data)->f, 3);
        break;
    case RIZZ_REFL_VARIANTTYPE_MAT4:
        sx_assert(size == sizeof(sx_mat4));
        cj5_seekget_array_float(r, jctx->cur_token, name, ((sx_mat4*)data)->f, 16);
        break;
    case RIZZ_REFL_VARIANTTYPE_MAT3:
        sx_assert(size == sizeof(sx_mat3));
        cj5_seekget_array_float(r, jctx->cur_token, name, ((sx_mat3*)data)->f, 9);
        break;
    case RIZZ_REFL_VARIANTTYPE_VEC4:
        sx_assert(size == sizeof(sx_vec4));
        cj5_seekget_array_float(r, jctx->cur_token, name, ((sx_vec4*)data)->f, 4);
        break;
    case RIZZ_REFL_VARIANTTYPE_VEC2:
        sx_assert(size == sizeof(sx_vec2));
        cj5_seekget_array_float(r, jctx->cur_token, name, ((sx_vec2*)data)->f, 2);
        break;
    case RIZZ_REFL_VARIANTTYPE_IVEC2:
        sx_assert(size == sizeof(sx_ivec2));
        cj5_seekget_array_int(r, jctx->cur_token, name, ((sx_ivec2*)data)->n, 2);
        break;
    case RIZZ_REFL_VARIANTTYPE_CHAR: {
        sx_assert(size == sizeof(char));
        char str[2];
        cj5_seekget_array_float(r, jctx->cur_token, name, ((sx_mat3*)data)->f, 9);
        *((char*)data) = cj5_seekget_string(r, jctx->cur_token, name, str, 2, "")[0];
    }   break;
    case RIZZ_REFL_VARIANTTYPE_COLOR: {
        int c[4];
        cj5_seekget_array_int(r, jctx->cur_token, name, c, 4);
        *((sx_color*)data) = sx_color4u((uint8_t)c[0], (uint8_t)c[1], (uint8_t)c[2], (uint8_t)c[3]);
    }   break;
    case RIZZ_REFL_VARIANTTYPE_UINT32:
        *((uint32_t*)data) = cj5_seekget_uint(r, jctx->cur_token, name, 0);
        break;
    case RIZZ_REFL_VARIANTTYPE_AABB:
        sx_assert(size == sizeof(sx_aabb));
        cj5_seekget_array_float(r, jctx->cur_token, name, ((sx_aabb*)data)->f, 6);
        break;
    case RIZZ_REFL_VARIANTTYPE_RECT:
        sx_assert(size == sizeof(sx_rect));
        cj5_seekget_array_float(r, jctx->cur_token, name, ((sx_rect*)data)->f, 4);
        break;
    case RIZZ_REFL_VARIANTTYPE_DOUBLE:
        sx_assert(size == sizeof(double));
        *((double*)data) = cj5_seekget_double(r, jctx->cur_token, name, 0);
        break;
    case RIZZ_REFL_VARIANTTYPE_INT8:
        sx_assert(size == sizeof(int8_t));
        *((int8_t*)data) = (int8_t)cj5_seekget_int(r, jctx->cur_token, name, 0);
        break;
    case RIZZ_REFL_VARIANTTYPE_INT16:
        sx_assert(size == sizeof(int16_t));
        *((int16_t*)data) = (int16_t)cj5_seekget_int(r, jctx->cur_token, name, 0);
        break;
    case RIZZ_REFL_VARIANTTYPE_INT64:
        sx_assert(size == sizeof(int64_t));
        *((int64_t*)data) = cj5_seekget_int64(r, jctx->cur_token, name, 0);
        break;
    case RIZZ_REFL_VARIANTTYPE_UINT8:
        sx_assert(size == sizeof(uint8_t));
        *((uint8_t*)data) = (uint8_t)(cj5_seekget_uint(r, jctx->cur_token, name, 0) & 0xff);
        break;
    case RIZZ_REFL_VARIANTTYPE_UINT16:
        sx_assert(size == sizeof(uint16_t));
        *((uint16_t*)data) = (uint16_t)(cj5_seekget_uint(r, jctx->cur_token, name, 0) & 0xffff);
        break;
    case RIZZ_REFL_VARIANTTYPE_UINT64:
        sx_assert(size == sizeof(uint64_t));
        *((uint64_t*)data) = cj5_seekget_uint64(r, jctx->cur_token, name, 0);
        break;
    default:
        sx_assertf(0, "unsupported type");
        break;
    }
}

static void refl__deserialize_json_builtin_array(const char* name, void* data, rizz_refl_variant_type type, 
                                                 int count, int stride, void* user, const void* meta, 
                                                 bool last_in_parent)
{
    sx_unused(meta);

    refl__read_json_context* jctx = user;
    cj5_result* r = &jctx->json->result;
    int array_id = cj5_seek(r, jctx->cur_token, name);
    sx_assert(array_id != -1);

    sx_assert(r->tokens[array_id].size >= count);
    int elem_id = 0;
    for (int i = 0; i < count; i++) {
        elem_id = cj5_get_array_elem_incremental(r, array_id, i, elem_id);
        refl__deserialize_json_builtin(name, (uint8_t*)data + (size_t)stride*(size_t)i, type, stride, user, meta, last_in_parent);
    }
}

static void refl__deserialize_json_struct_begin(const char* name, const char* type_name, int size, 
                                                int count, void* user, const void* meta)
{
    sx_unused(meta);
    sx_unused(type_name);
    sx_unused(size);

    refl__read_json_context* jctx = user;
    cj5_result* r = &jctx->json->result;

    sx_assert_alwaysf(jctx->token_stack_idx < JSON_STACK_COUNT, 
                      "Maximum stack for json serilize context reached");
    jctx->token_stack[jctx->token_stack_idx++] = jctx->cur_token;
    jctx->cur_token = cj5_seek(r, jctx->cur_token, name);
    if (count > 1) {
        jctx->struct_array_parent = jctx->cur_token;
    }
}

static void refl__deserialize_json_struct_array_element(int index, void* user, const void* meta)
{
    sx_unused(meta);

    refl__read_json_context* jctx = user;
    cj5_result* r = &jctx->json->result;
    
    jctx->cur_token = cj5_get_array_elem(r, jctx->struct_array_parent, index);
}

static void refl__deserialize_json_struct_end(void* user, const void* meta, bool last_in_parent)
{
    sx_unused(meta);
    sx_unused(last_in_parent);

    refl__read_json_context* jctx = user;
    sx_assert(jctx->cur_token != -1);
    
    sx_assert_alwaysf(jctx->token_stack_idx > 0, "Token stack overflow: Possible invalid json");

    jctx->cur_token = jctx->token_stack[--jctx->token_stack_idx];
    jctx->struct_array_parent = -1;
}

static void refl__deserialize_json_enum(const char* name, int* out_value, void* user, const void* meta, 
                                        bool last_in_parent)
{
    sx_unused(meta);
    sx_unused(last_in_parent);

    refl__read_json_context* jctx = user;
    cj5_result* r = &jctx->json->result;
    char str[64];
    *out_value = refl__get_enum(jctx->rctx, cj5_seekget_string(r, jctx->cur_token, name, str, sizeof(str), ""), 0);
}

static bool rizz__refl_deserialize_json(rizz_refl_context* ctx, const char* type_name, void* data, 
                                        rizz_json* json, int root_token_id)
{
    sx_assert(json);
    sx_assert(data);

    refl__read_json_context jctx = {
        .rctx = ctx,
        .json = json,
        .cur_token = root_token_id
    };

    if (refl__deserialize(ctx, type_name, data, &jctx,
                        &(rizz_refl_deserialize_callbacks) {
                            .on_begin = refl__deserialize_json_begin,
                            .on_end = refl__deserialize_json_end,
                            .on_enum = refl__deserialize_json_enum,
                            .on_builtin = refl__deserialize_json_builtin,
                            .on_builtin_array = refl__deserialize_json_builtin_array,
                            .on_struct_begin = refl__deserialize_json_struct_begin,
                            .on_struct_end = refl__deserialize_json_struct_end,
                            .on_struct_array_element = refl__deserialize_json_struct_array_element }))
    {
        return true;
    }

    return false;
}

static sx_mem_block* rizz__refl_serialize_json(rizz_refl_context* ctx, const char* type_name, 
                                               const void* data, const sx_alloc* alloc, bool prettify)
{
    sx_assert(data);
    sx_assert(alloc);

    refl__write_json_context jctx = {
        .alloc = alloc,
        .newline = prettify ? "\n" : NULL,
        .tab = prettify ? "\t" : NULL,
    };
    sx_mem_init_writer(&jctx.writer, alloc, 4096);

    sx_mem_block* mem = NULL;
    if (refl__serialize(ctx, type_name, data, &jctx,
                          &(rizz_refl_serialize_callbacks) {
                              .on_begin = refl__serialize_json_begin,
                              .on_end = refl__serialize_json_end,
                              .on_builtin = refl__serialize_json_builtin,
                              .on_builtin_array = refl__serialize_json_builtin_array,
                              .on_struct_begin = refl__serialize_json_struct_begin,
                              .on_struct_array_element = refl__serialize_json_struct_array_element,
                              .on_struct_end = refl__serialize_json_struct_end,
                              .on_enum = refl__serialize_json_enum }))
    {
        mem = jctx.writer.mem;
        mem->size = jctx.writer.top;
        sx_mem_addref(mem);
    }

    sx_mem_release_writer(&jctx.writer);
    return mem;
}

rizz_api_refl the__refl = { .create_context = refl__create_context,
                            .destroy_context = refl__destroy_context,
                            ._reg_private = refl__reg,
                            .size_of = refl__size_of,
                            .get_func = refl__get_func,
                            .get_enum = refl__get_enum,
                            .get_field = refl__get_field,
                            .get_enum_name = rizz__refl_get_enum_name,
                            .reg_count = refl__reg_count,
                            .deserialize = refl__deserialize,
                            .serialize = refl__serialize,
                            .get_fields = refl__get_fields,
                            .serialize_json = rizz__refl_serialize_json,
                            .deserialize_json = rizz__refl_deserialize_json };
