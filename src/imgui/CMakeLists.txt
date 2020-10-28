cmake_minimum_required(VERSION 3.1)
project(imgui)

set(imgui_sources imgui.c 
                  log-window.c 
				  imgui-internal.h
				  imguizmo-wrapper.c 
                  ../../include/rizz/imgui.h
                  ../../include/rizz/imgui-extra.h
                  README.md)
rizz_add_plugin(imgui "${imgui_sources}")
target_link_libraries(imgui PRIVATE cimgui imguizmo)

set(shaders imgui.vert imgui.frag)
set_source_files_properties(${shaders} PROPERTIES GLSLCC_OUTPUT_DIRECTORY "shaders_h" 
                                                  GLSLCC_SOURCE_GROUP "shaders")
glslcc_target_compile_shaders_h(imgui "${shaders}")
