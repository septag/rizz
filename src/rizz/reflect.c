//
// Copyright 2019 Sepehr Taghdisian (septag@github). All rights reserved.
// License: https://github.com/septag/rizz#license-bsd-2-clause
//

#include "rizz/reflect.h"
#include "rizz/core.h"

#include "sx/allocator.h"
#include "sx/array.h"
#include "sx/hash.h"
#include "sx/string.h"

#include <alloca.h>

#define DEFAULT_REG_SIZE 512

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

typedef struct rizz__reflect_context {
    rizz__refl_struct* structs;
    rizz__refl_enum* enums;
    rizz__refl_data* regs;    // sx_array
    const sx_alloc* alloc;
    sx_hashtbl* reg_tbl;    // refl.name --> index(regs)
    int max_regs;           // =0 if unlimited
} rizz__reflect_context;

static rizz__reflect_context g_reflect;

bool rizz__refl_init(const sx_alloc* alloc, int max_regs)
{
    g_reflect.max_regs = max_regs;
    g_reflect.alloc = alloc;

    if (max_regs > 0) {
        sx_array_add(alloc, g_reflect.regs, max_regs);
        sx_array_clear(g_reflect.regs);
    }

    g_reflect.reg_tbl =
        sx_hashtbl_create(alloc, (max_regs <= 0) ? (DEFAULT_REG_SIZE << 1) : (max_regs << 1));
    if (!g_reflect.reg_tbl)
        return false;

    return true;
}

void rizz__refl_release()
{
    if (g_reflect.alloc) {
        const sx_alloc* alloc = g_reflect.alloc;
        if (g_reflect.reg_tbl)
            sx_hashtbl_destroy(g_reflect.reg_tbl, alloc);
        for (int i = 0; i < sx_array_count(g_reflect.enums); i++) {
            sx_array_free(g_reflect.alloc, g_reflect.enums[i].name_ids);
        }
        sx_array_free(alloc, g_reflect.regs);
        sx_array_free(alloc, g_reflect.structs);
        sx_array_free(alloc, g_reflect.enums);

        g_reflect.alloc = NULL;
    }
}

// clang-format off
static inline int rizz__refl_type_size(const char* type_name) {
    if (sx_strequal(type_name, "int"))              return sizeof(int);
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
    else                                            return 0;
}
// clang-format on

static void* rizz__refl_get_func(const char* name)
{
    int index = sx_hashtbl_find_get(g_reflect.reg_tbl, sx_hash_fnv32_str(name), -1);
    return (index != -1) ? (g_reflect.regs[index].r.any) : NULL;
}

static int rizz__refl_get_enum(const char* name, int not_found)
{
    int index = sx_hashtbl_find_get(g_reflect.reg_tbl, sx_hash_fnv32_str(name), -1);
    return (index != -1) ? (int)g_reflect.regs[index].r.offset : not_found;
}

static const char* rizz__refl_get_enum_name(const char* type, int val)
{
    for (int i = 0, c = sx_array_count(g_reflect.enums); i < c; i++) {
        if (sx_strequal(g_reflect.enums[i].type, type)) {
            int* name_ids = g_reflect.enums[i].name_ids;
            for (int k = 0, kc = sx_array_count(name_ids); k < kc; k++) {
                const rizz__refl_data* r = &g_reflect.regs[name_ids[k]];
                sx_assert(r->r.internal_type == RIZZ_REFL_ENUM);
                if (val == (int)r->r.offset)
                    return r->name;
            }
        }
    }
    return "";
}

static void* rizz__refl_get_field(const char* base_type, void* obj, const char* name)
{
    int len = sx_strlen(name) + sx_strlen(base_type) + 2;
    char* base_name = (char*)alloca(len);
    sx_assert(base_name);
    sx_snprintf(base_name, len, "%s.%s", base_type, name);
    int index = sx_hashtbl_find_get(g_reflect.reg_tbl, sx_hash_fnv32(name, (size_t)len - 1), -1);
    return (index != -1) ? ((uint8_t*)obj + g_reflect.regs[index].r.offset) : NULL;
}

static void rizz__refl_reg(rizz_refl_type internal_type, void* any, const char* type,
                           const char* name, const char* base, const char* desc, int size,
                           int base_size)
{
    if (g_reflect.max_regs > 0) {
        int count = sx_array_count(g_reflect.regs);
        sx_assert(g_reflect.max_regs > count);
        if (g_reflect.max_regs > count) {
            rizz_log_warn("maximum amount of reflection regs exceeded");
            return;
        }
    }

    int id = sx_array_count(g_reflect.regs);
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
            for (int i = 0, c = sx_array_count(g_reflect.enums); i < c; i++) {
                if (sx_strequal(g_reflect.enums[i].type, type)) {
                    found_idx = i;
                    break;
                }
            }

            if (found_idx != -1) {
                rizz__refl_enum* _enum = &g_reflect.enums[found_idx];
                sx_array_push(g_reflect.alloc, _enum->name_ids, id);
            } else {
                rizz__refl_enum _enum = (rizz__refl_enum){ .name_ids = NULL };
                sx_strcpy(_enum.type, sizeof(_enum.type), type);
                sx_array_push(g_reflect.alloc, _enum.name_ids, id);
                sx_array_push(g_reflect.alloc, g_reflect.enums, _enum);
            }
        }    // RIZZ_REFL_ENUM

        key = name;
    }

    // registery must not exist
    if (sx_hashtbl_find(g_reflect.reg_tbl, sx_hash_fnv32_str(key)) >= 0) {
        rizz_log_warn("'%s' is already registered for reflection", key);
        return;
    }

    rizz__refl_data r = { .r =
                              {
                                  .any = any,
                                  .desc = desc,
                                  .size = size,
                                  .array_size = 1,
                                  .stride = size,
                                  .internal_type = internal_type,
                              },
                          .base_id = -1 };
    sx_strcpy(r.name, sizeof(r.name), name);
    if (base)
        sx_strcpy(r.base, sizeof(r.base), base);

    // check for array types []
    const char* bracket = sx_strchar(type, '[');
    if (bracket) {
        // remove whitespace
        while (bracket != type && sx_isspace(*(--bracket)))
            ;
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

    for (int i = 0, c = sx_array_count(g_reflect.structs); i < c; i++) {
        if (sx_strequal(g_reflect.structs[i].type, r.type)) {
            r.r.flags |= RIZZ_REFL_FLAG_IS_STRUCT;
            if (r.r.flags & RIZZ_REFL_FLAG_IS_ARRAY) {
                r.r.array_size = size / g_reflect.structs[i].size;
                r.r.stride = g_reflect.structs[i].size;
            }
            break;
        }
    }

    // determine size of array elements (built-in types)
    int stride = rizz__refl_type_size(r.type);
    if ((r.r.flags & RIZZ_REFL_FLAG_IS_ARRAY) && !(r.r.flags & RIZZ_REFL_FLAG_IS_STRUCT)) {
        sx_assert(stride > 0 && "invalid built-in type for array");
        r.r.array_size = size / stride;
        r.r.stride = stride;
    }

    if (!stride && !(r.r.flags & RIZZ_REFL_FLAG_IS_STRUCT)) {
        // it's probably an enum
        for (int i = 0, c = sx_array_count(g_reflect.enums); i < c; i++) {
            if (sx_strequal(g_reflect.enums[i].type, type)) {
                r.r.flags |= RIZZ_REFL_FLAG_IS_ENUM;
                break;
            }
        }
    }

    // check for base type (struct) and assign the base_id
    if (base) {
        int base_id = -1;
        // search to see if we already have the base type
        for (int i = 0, c = sx_array_count(g_reflect.structs); i < c; i++) {
            rizz__refl_struct* s = &g_reflect.structs[i];
            if (sx_strequal(s->type, base)) {
                base_id = i;
                break;
            }
        }

        if (base_id == -1) {
            rizz__refl_struct _base = (rizz__refl_struct){ .size = base_size };
            sx_strcpy(_base.type, sizeof(_base.type), base);
            sx_array_push(g_reflect.alloc, g_reflect.structs, _base);
            base_id = sx_array_count(g_reflect.structs) - 1;
        }

        r.base_id = base_id;
        ++g_reflect.structs[base_id].num_fields;
    }

    //
    sx_array_push(g_reflect.alloc, g_reflect.regs, r);

    if (g_reflect.max_regs <= 0 &&
        g_reflect.reg_tbl->count > (g_reflect.reg_tbl->capacity * 2 / 3)) {
        if (!sx_hashtbl_grow(&g_reflect.reg_tbl, g_reflect.alloc)) {
            rizz_log_warn("refl: could not grow the hash-table");
            return;
        }
    }

    sx_hashtbl_add(g_reflect.reg_tbl, sx_hash_fnv32_str(key), id);
}

static int rizz__refl_size_of(const char* base_type)
{
    for (int i = 0, c = sx_array_count(g_reflect.structs); i < c; i++) {
        if (sx_strequal(g_reflect.structs[i].type, base_type))
            return g_reflect.structs[i].size;
    }

    return 0;
}

static int rizz__refl_get_fields(const char* base_type, void* obj, rizz_refl_field* fields,
                                 int max_fields)
{
    int num_fields = 0;
    for (int i = 0, c = sx_array_count(g_reflect.regs); i < c; i++) {
        rizz__refl_data* r = &g_reflect.regs[i];
        if (r->r.internal_type == RIZZ_REFL_FIELD && sx_strequal(r->base, base_type)) {
            sx_assert(r->base_id < sx_array_count(g_reflect.structs));
            sx_assert(g_reflect.structs);
            rizz__refl_struct* s = &g_reflect.structs[r->base_id];

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
                                         .internal_type = r->r.internal_type };

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

static int rizz__refl_reg_count()
{
    return sx_array_count(g_reflect.regs);
}

static bool rizz__refl_is_cstring(const rizz_refl_info* r)
{
    return sx_strequal(r->type, "char") && (r->flags & RIZZ_REFL_FLAG_IS_ARRAY);
}


rizz_api_refl the__refl = { ._reg = rizz__refl_reg,
                            .size_of = rizz__refl_size_of,
                            .get_func = rizz__refl_get_func,
                            .get_enum = rizz__refl_get_enum,
                            .get_enum_name = rizz__refl_get_enum_name,
                            .get_field = rizz__refl_get_field,
                            .get_fields = rizz__refl_get_fields,
                            .reg_count = rizz__refl_reg_count,
                            .is_cstring = rizz__refl_is_cstring };
