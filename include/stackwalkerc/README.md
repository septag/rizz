# stackwalkerc - Windows single header stack walker in C (DbgHelp.DLL)

## Features

- Can be used in C or C++ code
- Super simple API
- Single header library makes integration into projects easy and fast
- Overridable memory allocations and assertion
- Zero heap allocations in callstack function (sw_show_callstack)
- No extra header inclusions except lightweight *stdbool.h* and *stdint.h* for delclarations
- Reporting of loaded modules and symbol search paths of the executable
- Callstack reporting of other threads and processes
- Callstack reporting in system exceptions
- Does not depend on DbgHelp.DLL to be included with the executable, this library dynamically loads the DbgHelp.dll from common system paths

## Usage
This is a single header library, so all you need is include the file in your source and you are good to go:

```cpp
// SW_IMPL includes implementation as well, you can skip this if you only want the declarations
#define SW_IMPL
#include "stackwalker.h"
```

Optionally, you can override some base functionality like function visibility, assertion and malloc, before including *stackwalkerc.h*:

```cpp
#define SW_API_DECL static  // declars everything as static so only visible to current translation unit
#define SW_ASSERT(e)  MyAssert(e)       // override assert
#define SW_MALLOC(size)  MyMalloc(size) // override default stdc malloc
#define SW_FREE(ptr) MyFree(ptr)        //  override default stdc free
#define SW_IMPL
#include "stackwalker.h"
```

The API usage is super simple. There is more functionality like listing symbol search paths and loaded modules, for more detailed example, check out [example.cpp](example/example.cpp) for details.

```cpp
static void callstack_entry(const sw_callstack_entry* entry, void* userptr)
{
    printf("\t%s(%d): %s\n", entry->line_filename, entry->line, entry->und_fullname);
}

int main(int argc, char* argv[]) 
{
    sw_callbacks callbacks = {
        .callstack_entry = callstack_entry
    };
    sw_context* stackwalk = sw_create_context_capture(SW_OPTIONS_ALL, callbacks, NULL);
    if (!stackwalk) {
        puts("ERROR: stackwalk init");
        return -1;
    }
    sw_show_callstack(stackwalk, NULL); // Second parameter NULL means that stackwalker should resolve for current thread
    sw_destroy_context(g_stackwalk);
    return 0;
}
```

To build the example, just make sure you have the right compiler flags (build debug symbols) and also link with *Version.lib*. The main API will be loaded dyamically from *dbghelp.dll* in common system paths (see `sw__init_internal` to see the search paths for the DLL file):

MSVC:
```
cd example
cl /Od /Zi Version.lib example.cpp
```

Clang (win):
```
cd example 
clang -g -O0 -lVersion example.cpp -o example.exe
```


## Acknowledgments
Almost all of the Windows API usage for StackWalk are taken from [StackWalker](https://github.com/JochenKalmbach/StackWalker) project by *Jochen Kalmbach*. For detailed information on how to use the API, read the *Kalmbach's* article on [CodeProject](https://www.codeproject.com/Articles/11132/Walking-the-callstack-2).  
This is actually a much more simplified (and improved imo) and straight-to-the-point version of *StackWalker* library.   
This project only supports msvc2015+/clang(windows) compilers, if you prefer C++ API or want support for older Visual studio versions, check out Kalmbach's StackWalker library mentioned above.

[License (BSD 2-clause)](https://github.com/septag/stackwalkerc/blob/master/LICENSE)
--------------------------------------------------------------------------

<a href="http://opensource.org/licenses/BSD-2-Clause" target="_blank">
<img align="right" src="http://opensource.org/trademarks/opensource/OSI-Approved-License-100x137.png">
</a>

	Copyright 2021 Sepehr Taghdisian. All rights reserved.
	
	https://github.com/septag/stackwalker.c
	
	Redistribution and use in source and binary forms, with or without
	modification, are permitted provided that the following conditions are met:
	
	   1. Redistributions of source code must retain the above copyright notice,
	      this list of conditions and the following disclaimer.
	
	   2. Redistributions in binary form must reproduce the above copyright notice,
	      this list of conditions and the following disclaimer in the documentation
	      and/or other materials provided with the distribution.
	
	THIS SOFTWARE IS PROVIDED BY COPYRIGHT HOLDER ``AS IS'' AND ANY EXPRESS OR
	IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
	MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
	EVENT SHALL COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
	INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
	BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
	DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
	LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
	OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
	ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
