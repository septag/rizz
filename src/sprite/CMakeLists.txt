cmake_minimum_required(VERSION 3.1)
project(sprite)

set(sprite_sources sprite.c 
                   ../../include/rizz/sprite.h
                   README.md)
rizz_add_plugin(sprite "${sprite_sources}")

set(shaders sprite.vert sprite.frag)
set_source_files_properties(${shaders} PROPERTIES GLSLCC_OUTPUT_DIRECTORY "shaders_h")
glslcc_target_compile_shaders_h(sprite "${shaders}")

# recompile sprite shaders with WIREFRAME flag
set_source_files_properties(${shaders} PROPERTIES COMPILE_DEFINITIONS "WIREFRAME" 
                                                  GLSLCC_OUTPUT_FILENAME "sprite_wire")
glslcc_target_compile_shaders_h(sprite "${shaders}")