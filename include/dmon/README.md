## dmon
[@septag](https://twitter.com/septagh)  

_dmon_ is a tiny C library that monitors changes in a directory.
It provides a unified solution to multiple system APIs that exist for each OS. It can also monitor directories recursively. 

### Platforms
- Windows: `ReadDirectoryChangesW` backend. Tested with Windows10 SDK + Visual Studio 2019
- Linux: `inotify` backend. Tested with gcc-7.4/clang-6, ubuntu 18.04 LTS
- MacOS: `FSEvents` backend. Tested with MacOS-10.14 clang 10

### Usage

You just have to include the file and use it's functions. It is also compatible with C++ code.
Backslashes in Windows paths are also converted to '/' for portability.

```c
#define DMON_IMPL
#include "dmon.h"

static void watch_callback(dmon_watch_id watch_id, dmon_action action, const char* rootdir,
                           const char* filepath, const char* oldfilepath, void* user)
{
    // receive change events. type of event is stored in 'action' variable
}

int main() 
{
    dmon_init();
    dmon_watch("/path/to/directory", watch_callback, DMON_WATCHFLAGS_RECURSIVE, NULL); 
    // wait ...
    dmon_deinit();
	return 0;
}
```

For more information and how to customize functionality, see [dmon.h](dmon.h)

To build on linux, link with `pthread`:
```gcc test.c -lpthread -o test```

To build on MacOS, link with `CoreServices` and `CoreFoundation`:
```clang test.c -framework CoreFoundation -framework CoreServices -lpthread -o test```

[License (BSD 2-clause)](https://github.com/septag/dmon/blob/master/LICENSE)
--------------------------------------------------------------------------

<a href="http://opensource.org/licenses/BSD-2-Clause" target="_blank">
<img align="right" src="http://opensource.org/trademarks/opensource/OSI-Approved-License-100x137.png">
</a>

	Copyright 2019 Sepehr Taghdisian. All rights reserved.
	
	https://github.com/septag/dmon
	
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
