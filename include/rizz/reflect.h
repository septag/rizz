//
// Copyright 2019 Sepehr Taghdisian (septag@github). All rights reserved.
// License: https://github.com/septag/rizz#license-bsd-2-clause
//
//
// Register structure members:
//      struct sx_vec3  {   float x, y, z;  }
//      rizz_refl_field(refl_api, sx_vec3, float, x, "x axis");
//      rizz_refl_field(refl_api, sx_vec3, float, y, "y axis");
//      rizz_refl_field(refl_api, sx_vec3, float, z, "z axis");
// Register functions:
//      void foo(int n);
//      rizz_refl_func(refl_api, void(int n), foo, "function foo")
// Register enums
//      enum my_enum { ENUM1, ENUM2 }
//      rizz_refl_enum(refl_api, my_enum, ENUM1)
//      rizz_refl_enum(refl_api, my_enum, ENUM2)
//
// Rules:
//      - only use macros to register reflection types, not `_reg` function
//      - For array member types, always use known standard types: int, float, ..
//      - Registered struct types are allowed for array member types too
//
#pragma once

#include "types.h"

typedef struct sx_alloc sx_alloc;

typedef enum rizz_refl_type { RIZZ_REFL_ENUM, RIZZ_REFL_FUNC, RIZZ_REFL_FIELD } rizz_refl_type;

enum rizz_refl_flags_ {
    RIZZ_REFL_FLAG_IS_PTR = 0x1,       // field is pointer
    RIZZ_REFL_FLAG_IS_STRUCT = 0x2,    // field is struct type
    RIZZ_REFL_FLAG_IS_ARRAY = 0x4,     // field is array (only built-in types are supported)
    RIZZ_REFL_FLAG_IS_ENUM = 0x8       // field is enum
};
typedef uint32_t rizz_refl_flags;

typedef struct rizz_refl_info {
    union {
        void*    any;
        intptr_t offset;
    };

    const char*    type;
    const char*    name;
    const char*    base;
    const char*    desc;
    int            size;
    int            array_size;
    int            stride;
    uint32_t       flags;
    rizz_refl_type internal_type;
} rizz_refl_info;

typedef struct rizz_refl_field {
    rizz_refl_info info;
    void*          value;
} rizz_refl_field;

typedef struct rizz_api_refl {
    void (*_reg)(rizz_refl_type internal_type, void* any, const char* type, const char* name,
                 const char* base, const char* desc, int size, int base_size);
    int (*size_of)(const char* base_type);
    void* (*get_func)(const char* name);
    int (*get_enum)(const char* name, int not_found);
    const char* (*get_enum_name)(const char* type, int val);
    void* (*get_field)(const char* base_type, void* obj, const char* name);
    int (*get_fields)(const char* base_type, void* obj, rizz_refl_field* fields, int max_fields);
    int (*reg_count)();
    bool (*is_cstring)(const rizz_refl_info* r);
} rizz_api_refl;


#ifdef RIZZ_INTERNAL_API
bool rizz__refl_init(const sx_alloc* alloc, int max_regs sx_default(0));
void rizz__refl_release();

RIZZ_API rizz_api_refl the__refl;

#    define rizz_refl_enum(_type, _name)                                                 \
        the__refl._reg(RIZZ_REFL_ENUM, (void*)(intptr_t)_name, #_type, #_name, NULL, "", \
                       sizeof(_type), 0)
#    define rizz_refl_func(_type, _name, _desc) \
        the__refl._reg(RIZZ_REFL_FUNC, &_name, #_type, #_name, NULL, _desc, sizeof(void*), 0)
#    define rizz_refl_field(_struct, _type, _name, _desc)                                         \
        the__refl._reg(RIZZ_REFL_FIELD, &(((_struct*)0)->_name), #_type, #_name, #_struct, _desc, \
                       sizeof(_type), sizeof(_struct))
#else
#    define rizz_refl_enum(_api, _type, _name)                                       \
        _api->_reg(RIZZ_REFL_ENUM, (void*)(intptr_t)_name, #_type, #_name, NULL, "", \
                   sizeof(_type), 0)
#    define rizz_refl_func(_api, _type, _name, _desc) \
        _api->_reg(RIZZ_REFL_FUNC, &_name, #_type, #_name, NULL, _desc, sizeof(void*), 0)
#    define rizz_refl_field(_api, _struct, _type, _name, _desc)                               \
        _api->_reg(RIZZ_REFL_FIELD, &(((_struct*)0)->_name), #_type, #_name, #_struct, _desc, \
                   sizeof(_type), sizeof(_struct))
#endif
