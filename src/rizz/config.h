//
// Copyright 2019 Sepehr Taghdisian (septag@github). All rights reserved.
// License: https://github.com/septag/rizz#license-bsd-2-clause
//
#pragma once

#include "sx/platform.h"

#define RIZZ_VERSION 200

// enables hot-loading for assets, plugins (+game) and shaders
#ifndef RIZZ_CONFIG_HOT_LOADING
#    if SX_PLATFORM_ANDROID || SX_PLATFORM_IOS
#        define RIZZ_CONFIG_HOT_LOADING 0
#    else
#        define RIZZ_CONFIG_HOT_LOADING 1
#    endif
#endif    // RIZZ_CONFIG_HOT_LOADING

// Time to query file changes of each plugin
#ifndef RIZZ_CONFIG_PLUGIN_UPDATE_INTERVAL
#    define RIZZ_CONFIG_PLUGIN_UPDATE_INTERVAL 1.0f
#endif

#ifndef RIZZ_CONFIG_ASSET_POOL_SIZE
#    define RIZZ_CONFIG_ASSET_POOL_SIZE 256
#endif

#ifndef RIZZ_CONFIG_MAX_HTTP_REQUESTS
#    define RIZZ_CONFIG_MAX_HTTP_REQUESTS 32
#endif

#ifndef RIZZ_CONFIG_MAX_DEBUG_VERTICES
#    define RIZZ_CONFIG_MAX_DEBUG_VERTICES 10000
#endif

#ifndef RIZZ_CONFIG_MAX_DEBUG_INDICES
#    define RIZZ_CONFIG_MAX_DEBUG_INDICES 10000
#endif

#ifndef RIZZ_CONFIG_DEBUG_MEMORY
#    ifdef _DEBUG
#        define RIZZ_CONFIG_DEBUG_MEMORY 1
#    else
#        define RIZZ_CONFIG_DEBUG_MEMORY 0
#    endif
#endif

