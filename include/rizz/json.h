#pragma once

#ifdef RIZZ_BUNDLE
#   include "sjson/sjson.h"
#else
#   include "sx/allocator.h"
#   include "sx/string.h"
#   define sjson_malloc(user, size)        sx_malloc((const sx_alloc*)(user), (size))
#   define sjson_free(user, ptr)           sx_free((const sx_alloc*)(user), (ptr))
#   define sjson_realloc(user, ptr, size)  sx_realloc((const sx_alloc*)(user), (ptr), (size))
#   define sjson_assert(e)                 sx_assert(e)
#   define sjson_out_of_memory()           sx_out_of_memory()
#   define sjson_snprintf                  sx_snprintf
#   define sjson_strlen(str)               sx_strlen((str))
#   define sjson_strcpy(a, s, b)           sx_strcpy((a), (s), (b))
#   define SJSON_IMPLEMENT
#   include "sjson/sjson.h"
#endif