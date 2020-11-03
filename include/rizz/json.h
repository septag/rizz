#pragma once

#if defined(RIZZ_BUNDLE) || defined(CJ5_IGNORE_IMPLEMENT)
#    include "cj5/cj5.h"
#else
#    define CJ5_ASSERT(e) sx_assert(e);
#    define CJ5_IMPLEMENT
#    include "cj5/cj5.h"
#endif

// underlying object for "json" asset type
typedef struct rizz_json {
    cj5_result result;
    void* user;
} rizz_json;

