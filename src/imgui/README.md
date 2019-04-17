## ImGUI plugin

The plugin for [dear imgui](https://github.com/ocornut/imgui) UI system. 
See [rizz_api_imgui](https://github.com/septag/rizz/blob/1833c4c716e9eba0fd8f774414f4a619f12b3999/include/imgui/imgui.h#L871) for API listing. 
It's essentially same as imgui but with no default parameters.

However, there are some other utility functions declared in [imgui-extra.h](https://github.com/septag/rizz/blob/master/include/imgui/imgui-extra.h):

- `memory_debugger` shows internal engine memory stats.
- `graphics_debugger` shows *sokol_gfx* API introspection and debugging information.
- `begin_fullscreen_draw` start fullscreen drawing. After this call, you can fetch the `ImDrawList` and begin debug drawing with imgui's `ImDrawList_` functions.
- `project_to_screen` helper to convert world to screen coordinates
- `gizmo_xxx` 3D gizmo. wrapper over [ImGuizmo](https://github.com/CedricGuillemet/ImGuizmo)