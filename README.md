[![Build Status](https://travis-ci.org/septag/rizz.svg?branch=master)](https://travis-ci.org/septag/rizz)

## Rizz
[@septag](https://twitter.com/septagh)  

Rizz (ریز) is a tiny, multi-platform, and minimal game/app development framework, Written in C language.  
It's currently a work in progress, features and improvements will be added constantly to different platforms.

## Design Principles
I'm not gonna waste time by rewriting the same stuff that others put it a lot better than me.  
Design principles of _rizz_ is pretty much same as 
[The Machinery's](https://ourmachinery.com/files/guidebook.md.html#omg-design:designprinciples). 
I will try to stick to these as much as possible, and I recommend every programmer to practice these principles too.

#### Note
This is not a game engine, it's a relatively low-level framework for programmers to build their own engine/renderer/physics on top of it. The core of _rizz_ does not and will not implement any rendering techniques/physics or impose any specific entity system to the user. It just provides the basic building blocks for game developers. Other features will be implemented as plugins.
  
## Features
#### Core
- *Portable C code*: C11 (gcc/clang), C99 (msvc) compatible code, designed with data-oriented mindset. 
- *Plugin system*: Engine has a small core. Many functionalities are implemented through plugins.
- *Minimal Dependencies*: No external/large dependencies. Only a handful of small dependencies included in the source.
- *Hot-reloading of C/C++ code*: Plugins/Game code are all hot-reloadable with some restrictions and rules.
- *Fiber based job system*: Simple to use fiber-based job system.
- *Reflection*: Provides simple reflection system for _structs_, _enums_ and _functions_.
- *Async Asset Manager*: Flexible reference counting asset manager. New asset types can be added by third-party code to the manager.
- *Hot-reloading of assets and shaders*: All in-game resources and shaders can be hot-reloaded.
- *Virtual file system*: Async read/write. Directories or archives can be mounted as virtual directories.
- *Support for coroutines*: Coroutines can be suspended for N frames or N milliseconds.

#### Graphics
- *Multiple graphics API support*: Metal (iOS, MacOS). OpenGL-ES 2/3 (Android). Direct3D11 (Windows), OpenGL 3.3 (Linux)
- *Portable shaders*: Write shaders once in GLSL, toolset will translate the shader to other platform APIs.
- *Multi-threaded GPU command-buffer*: Draw commands can be submitted by multiple threads.

#### Plugins
- [imgui](src/imgui): Dear-imgui plugin with some utility API
- [sprite](src/sprite): 2D Sprite system
- [sound](src/sound): Simple sound system 
- [input](src/input): Input system with gamepad and touch support

#### Debugging and Profiling
- *Remote Profiler*: Integrated *Remotery* for remote debugger/command console and log viewer.
- *Graphics API introspection*: Debug application level graphic calls and objects.
- *Memory Debugger*: Debug and monitor memory allocations for all subsystems.

#### Supported platforms
- *Windows*
- *Linux*
- *MacOS*
- *Android*
- *RaspberryPI*
- *iOS*: WIP v0.2

## Build
_rizz_ is designed to run on all major mobile (iOS, android), PC (Windows, Linux, MacOS) and web (WebASM) platforms. 
But as the engine is in it's early age, the current platforms are built and tested: 

- __Windows__: Tested on Windows10 with visual studio 14 2015 update 3 (Win64).  
- __Linux__: Tested on ubuntu 16 with clang (6.0.0) and gcc (7.3.0). Package requirements:  
  - libx11-dev
  - libxrandr-dev
  - libxi-dev
  - libasound2-dev (if you are planning to build `sound` plugin)
- __MacOS__: Tested on MacOS High Sierra - AppleClang 9.1.0
- __Android__: For android, there is a python script [android.py](scripts/build-tools/android.py) 
               which takes care of preparing android project structure, building the code and 
               packaging the final APK. please read the begining of ```android.py```.
- __RaspberryPI__: Tested on RPi1 ModelB Ubuntu-jessie (gcc Raspbian 4.9.2). Package requirements:
  - libasound2-dev (if you are planning to build `sound` plugin)

#### CMake options
- **BUNDLE** (default=0, android/ios=1):  
    - `BUNDLE=0` indicates that _rizz_ is built as an executable host which runs the game 
    by `rizz --run game.dll` (on linux it's `rizz --run ./game.so`). Recommended for development, 
    where you need reduced binary sizes and live-reloading of game code and plugins.  
    - `BUNDLE=1` Builds _rizz_ as static library. To link and bundle _rizz_ and other plugins with a 
    single stand-alone executable, so there will be only one executable and bundles _rizz_ and all the
    plugins you specify. to build the bundle properly, you should set these cmake arguments on configure:
		- **BUNDLE_TARGET**: target name of the executable you are trying to build (first example: `-DBUNDLE_TARGET=01-hello`)
		- **BUNDLE_TARGET_NAME**: if the _cmake_ target and the actual name of your application differs, use 
		                          argument to address that. (first example: `-DBUNDLE_TARGET_NAME=hello`)
		- **BUNDLE_PLUGINS**: list the plugins that is required by your application, separated by semicolon. 
		                      (first example: `-DBUNDLE_PLUGINS=imgui`)
								 
- **ENABLE_HOT_LOADING** (default=1, android/ios=0)
  Enables hot reloading of assets and monitoring the assets directories. Doesn't work on mobile OSes.
- **ENABLE_PROFILER** (default=0/debug, default=1/release)
- **BUILD_EXAMPLES** (default=1, android/ios=0)
  Build example projects in `/examples` directory. 
- **MSVC_COMPILE_SUMMARY** (default=0, windows/msvc=1)
  On msvc compiler, enables `/d2cgsummary` flag for detailed compile stats. Read more about this 
  [here](https://aras-p.info/blog/2017/10/23/Best-unknown-MSVC-flag-d2cgsummary/)

## Usage
To build a compatible game/app module for _rizz_ you should do the following steps:
- Add `rizz_plugin_decl_main(proj_name, plugin, event) {}` to your source. This is actually the main 
  function of your app. Main events like `STEP`, `INIT` and `SHUTDOWN` gets posted to this function and
  can be enumerated with `event` parameter.
- Add `rizz_plugin_implement_info(proj_name, version, "Title", 0);` to your source file. Every plugin
  needs to implement this in order to get recognized by _rizz_.
- Add `rizz_game_decl_config(conf) {}` to your source, and modify the config (see `rizz_config`) before
  initialization.
- To Receive windows events, Add `rizz_plugin_decl_event_handler(proj_name, event) {}` to your source file.
  _rizz_ will fill `rizz_app_event` struct and pass window messages to your application.
- The low-level graphics api is almost as same as [sokol_gfx](https://github.com/floooh/sokol/blob/master/sokol_gfx.h).
  So all documentation and examples of *sokol_gfx* also applies here, except the little concept called _stages_ that is 
  explained in [graphics.h](https://github.com/septag/rizz/blob/master/include/rizz/graphics.h) in more detail.

Currently, the documentation is lacking. But you can check out the `/examples` directory and figure out 
how to get started.

## Open-Source libraries used
#### Primarily developed for rizz
- [sx](https://github.com/septag/sx): Portable base library
- [glslcc](https://github.com/septag/glslcc): GLSL cross-compiler
- [dds-ktx](https://github.com/septag/dds-ktx): Single header KTX/DDS reader
- [sjson](https://github.com/septag/sjson): Fast and portable single-header C file Json encoder/decoder
- [atlasc](https://github.com/septag/atlasc): Command-line tool that builds atlas texture from a bunch of input images.

#### 3rdparties
- [sokol](https://github.com/floooh/sokol): minimal cross-platform standalone C headers
- [cr](https://github.com/fungos/cr): Simple C Hot Reload Header-only Library
- [cimgui](https://github.com/cimgui/cimgui): C-API for imgui
- [imgui](https://github.com/ocornut/imgui): Dear ImGui: Bloat-free Immediate Mode Graphical User interface for C++ with minimal dependencies
- [Remotery](https://github.com/Celtoys/Remotery): Single C file, Realtime CPU/GPU Profiler with Remote Web Viewer
- [lz4](https://github.com/lz4/lz4): Extremely Fast Compression algorithm
- [http](https://github.com/mattiasgustavsson/libs/blob/master/http.h): Basic HTTP protocol implementation over sockets
- [stb](https://github.com/nothings/stb): stb single-file public domain libraries for C/C++
- [sort](https://github.com/swenson/sort): Sorting routine implementations in "template" C
- [ImGuizmo](https://github.com/CedricGuillemet/ImGuizmo): 3D gizmo for imgui
- [gainput](https://github.com/jkuhlmann/gainput): Input library for games

[TODO](TODO.md)
----

[License (BSD 2-clause)](https://github.com/septag/rizz/blob/master/LICENSE)
--------------------------------------------------------------------------

<a href="http://opensource.org/licenses/BSD-2-Clause" target="_blank">
<img align="right" src="http://opensource.org/trademarks/opensource/OSI-Approved-License-100x137.png">
</a>

	Copyright 2019 Sepehr Taghdisian. All rights reserved.
	
	https://github.com/septag/rizz
	
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
