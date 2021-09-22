#include "sx/sx.h"

#if SX_PLATFORM_OSX
#include <cxxabi.h>

extern "C" char* rizz__demangle(const char* symbol)
{
    int status = 0;
    char* demangled = abi::__cxa_demangle(symbol, 0, 0, &status);
    if (status == 0) {
        return demangled;
    } else {
        return nullptr;
    }
}

#endif // SX_PLATFORM_OSX
