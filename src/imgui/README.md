## ImGUI plugin

The plugin for [dear imgui](https://github.com/ocornut/imgui) UI system. 
See [rizz_api_imgui](https://github.com/septag/rizz/blob/1833c4c716e9eba0fd8f774414f4a619f12b3999/include/rizz/imgui.h#L871) for API listing. 
It's essentially same as imgui but with no default parameters.

However, there are some other utility functions declared in [imgui-extra.h](https://github.com/septag/rizz/blob/master/include/rizz/imgui-extra.h):

- `memory_debugger` shows internal engine memory stats.
- `graphics_debugger` shows *sokol_gfx* API introspection and debugging information.
- `begin_fullscreen_draw` start fullscreen drawing. After this call, you can fetch the `ImDrawList` and begin debug drawing with imgui's `ImDrawList_` functions.
- `draw_cursor`: Draws ImGui cursor, in cases where you control the cursor yourself
- `project_to_screen`: helper to convert from world to screen coordinates
- `is_capturing_mouse`: returns true if imgui is using mouse
- `is_capturing_keyboard`: returns true if one of imgui controls are receiving input
- `dock_space_id`: returns dock space id (or zero if docking is disabled), which can be used inside the program to get different docking views, particularly useful for drawing in one of dock viewports
- `gizmo.xxx`: 3D transform/rotate/scale gizmo. wrapper over [ImGuizmo](https://github.com/CedricGuillemet/ImGuizmo)

### Configuration
- **rizz_config.imgui_docking**: set this field to _true_ in order to enable docking.
- **rizz.ini: [imgui] log_buffer_size**: set this field to define how much buffer to reserve for log window, in KB. (default: 64)