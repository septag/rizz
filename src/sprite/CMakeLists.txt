cmake_minimum_required(VERSION 3.1)
project(sprite)

add_library(sprite ${PLUGIN_TYPE} sprite.c 
                                  ../../include/rizz/sprite.h
                                  README.md)
target_link_libraries(sprite PRIVATE sx)
target_include_directories(sprite PRIVATE ../../3rdparty)

set(shaders sprite.vert sprite.frag)
set_source_files_properties(${shaders} PROPERTIES GLSLCC_OUTPUT_DIRECTORY "shaders_h")
glslcc_target_compile_shaders_h(sprite "${shaders}")

# recompile sprite shaders with WIREFRAME flag
set_source_files_properties(${shaders} PROPERTIES COMPILE_DEFINITIONS "WIREFRAME" 
                                                  GLSLCC_OUTPUT_FILENAME "sprite_wire")
glslcc_target_compile_shaders_h(sprite "${shaders}")