#pragma once

#ifdef RIZZ_BUNDLE
#    include "cj5/cj5.h"
#else
#    define CJ5_ASSERT(e) sx_assert(e);
#    define CJ5_IMPLEMENT
#    include "cj5/cj5.h"
#endif