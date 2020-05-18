# Changelog

## [0.41 (WIP)]
- [ ] EXPERIMENTAL: sokol debugging support for compute-shader
- [ ] EXPERIMENTAL: OpenGL compute-shader support
- [ ] Sprite flip flags (flipX/flipY)
- [ ] sprite anim/controller loading from data
- [x] sprite simpler draw api
- [ ] Implement multi-view and docking for UI and tools
- [ ] Add an API to render a set of sprites with a set of clips, so we won't have to duplicate data
- [ ] use <sys/sem.h> instead of posix semaphores
- [ ] Find a way to better streamline the sprite system and animations. AnimClips should just have frames in them and controlled by another state data
- [ ] Initiate 3dtools

## [0.4]
- [x] ADDED `shader_get`/`texture_get` and other asset getters to all asset types
- [x] ADDED basis texture support
- [x] ADDED true-type font (fontstash) support through 'rizz_api_font' API
- [x] CHANGED: Asset manager internal refactors (obj_threadsafe)
- [x] CHANGED: IFF load/save API in sx/io.h
- [x] CHANGED: Async vfs API tget individual files 
- [x] CHANGED sx io API and backend to native instead of std.fopen
- [x] UPDATED: to new `docking` branch of ImGui
- [x] UPDATED: sokol libs
- [x] UPDATED: imgui to 1.77-docking branch
- [x] UPDATED: Remotery
- [x] ADDED: imgui log window
- [x] BREAKING: external macro APIS (rizz_log_xxxx) are now same as internal ones (api variable is defined in header)
- [x] BREAKING: removed many rizz headers and joined them as one (rizz.h)
- [x] IMPROVEMENT: GPU command buffers multi-threaded submission are now more flexible, see basic design document
- [x] IMPROVEMENT: graphic helper functions/renderers can take any API (set_draw_api)
- [x] BREAKING: renamed _sprite_ plugin to _2dtools_. the api name is the same, but it's now part of '2dtools' plugin
- [x] BREAKING: replace sjson with cj5
- [x] FIXED: sx_matx_inv 
- [x] FIXED: Error handling for missing shaders so they won't crash
- [x] FIXED: crash on clang-cl and job dispatcher

## [0.2]
- [x] ADDED: Sprites (sprite.c)
- [x] ADDED: _drawsprite_ example
- [x] ADDED: Sprite animation/controller
- [x] ADDED: _animsprite_ example
- [x] ADDED: Sprite debugger
- [x] ADDED: Verbose logging
- [x] ADDED: Input system 
- [x] ADDED: CR update
- [x] ADDED: sokol update
- [x] ADDED: Initiate sound plugin
- [x] ADDED: _playsound_ example
- [x] ADDED: Gamepad support
- [x] ADDED: Gamepad demo
- [x] ADDED: Plugin dependencies
- [x] ADDED: Plugin APIs by name
- [x] ADDED: Cleanup and add something for examples cmake
- [x] ADDED: Add Readme to plugins (imgui, sprite)
- [x] ADDED: Better graphics debugging (stages)
- [x] ADDED: Audio backend for sokol_audio
- [x] ADDED: Android build
- [x] ADDED: Raspberry build
- [x] ADDED: iOS support
- [x] ADDED: Experimental D3D11 compute-shader support
- [x] ADDED: Nbody physics example (D3d11 only)
- [x] BREAKING - gfx: immediate and staged APIs are now the same. creation/destroy/query functions 
      are moved to `rizz_api_gfx`.
- [x] CHANGED: Now objects in use will never be destroyed even if the user calls _destroy_
- [x] FIXED: immediate mode api conflicts with imgui
- [x] FIXED: metal backend shader reload

## [0.1]:
- Asset manager
- Texture loader
- Graphics command-buffer with multi-threading support
- Shader hot-reload
- Texture hot-reload
- C/C++ hot-reload
- Plugin system
- ImGui integration
- Remotery profiler integration
- Basic Memory debugger
- Basic 2D debug primitives
- Customize ImGui font
- Assets runtime database (json)
- Tween
- Tiny FSM
- Tiny event queue
- Update imgui and incorporate tabs
- Graphics debugger (sokol)
- Demo project

