//
// Copyright 2019 Sepehr Taghdisian (septag@github). All rights reserved.
// License: https://github.com/septag/rizz#license-bsd-2-clause
//
#pragma once

// clang-format off
#include "sx/sx.h"

#ifdef __cplusplus
#   define RIZZ_API extern "C" 
#else
#   define RIZZ_API extern
#endif

#define rizz_to_id(_index)     ((uint32_t)(_index) + 1)
#define rizz_to_index(_id)     ((int)(_id) - 1)

// asset.h
// Internal object that can be casted to the actual asset object, can be an Id or pointer based on asset
// asset loaders must always assume that 'zero' value is invalid, wether it's an Id or pointer
typedef union  { uintptr_t id;  void* ptr; }    rizz_asset_obj;

typedef struct { uint32_t id; } rizz_asset;
typedef struct { uint32_t id; } rizz_asset_group;
typedef struct { uint32_t id; } rizz_http;
typedef struct { uint32_t id; } rizz_gfx_stage;

// clang-format on