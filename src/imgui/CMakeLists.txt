cmake_minimum_required(VERSION 3.1)
project(imgui)

add_library(imgui ${PLUGIN_TYPE} imgui.c 
                                 ../../include/imgui/imgui.h
                                 ../../include/imgui/imgui-extra.h)
target_link_libraries(imgui PRIVATE cimgui imguizmo sx)
target_include_directories(imgui PRIVATE ../../3rdparty)

set(files imgui.vert imgui.frag)
set_source_files_properties(${files} PROPERTIES GLSLCC_OUTPUT_DIRECTORY "shaders_h")
glslcc_target_compile_shaders_h(imgui "${files}")
