#include "sx/sx.h"

#if SX_PLATFORM_WINDOWS

#define WIN32_LEAN_AND_MEAN
#include <unknwn.h>

SX_PRAGMA_DIAGNOSTIC_PUSH()
SX_PRAGMA_DIAGNOSTIC_IGNORED_CLANG("-Wwritable-strings")
SX_PRAGMA_DIAGNOSTIC_IGNORED_CLANG("-Wshadow")
SX_PRAGMA_DIAGNOSTIC_IGNORED_CLANG("-Wundefined-internal")
SX_PRAGMA_DIAGNOSTIC_IGNORED_MSVC(4267)
SX_PRAGMA_DIAGNOSTIC_IGNORED_MSVC(4189)
SX_PRAGMA_DIAGNOSTIC_IGNORED_MSVC(4456)
SX_PRAGMA_DIAGNOSTIC_IGNORED_MSVC(4244)
#include "find_vstudio/find_vstudio.h"
SX_PRAGMA_DIAGNOSTIC_POP()

extern "C" bool rizz__win_get_vstudio_dir(char* vspath, size_t vspath_size)
{
    sx_assert(vspath);
    Find_Result r;
    find_visual_studio_by_fighting_through_microsoft_craziness(&r);

    if (r.vs_path) {
        memset(vspath, 0, vspath_size);
        WideCharToMultiByte(CP_UTF8, 0, r.vs_path, -1, vspath, (int)vspath_size, NULL, NULL);
    }

    free(r.windows_sdk_root);
    free(r.windows_sdk_um_library_path);
    free(r.windows_sdk_ucrt_library_path);
    free(r.vs_path);
    free(r.vs_exe_path);
    free(r.vs_library_path);

    return r.vs_path != NULL;
}

#endif // SX_PLATFORM_WINDOWS