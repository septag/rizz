cmake_minimum_required(VERSION 3.1)
project(2dtools)

set(2dtools_sources 2dtools.c 
                    sprite.c 
                    font.c 
                    2dtools-internal.h 
                    ../../include/rizz/2dtools.h
                    ../../3rdparty/fontstash/fontstash.h 
                    ../../3rdparty/fontstash/stb_truetype.h
                    README.md)
rizz_add_plugin(2dtools "${2dtools_sources}")

set(GLSLCC_OUTPUT_DIRECTORY "shaders_h")

# sprite shaders
set(sprite_shaders sprite.vert sprite.frag)
glslcc_target_compile_shaders_h(2dtools "${sprite_shaders}")

# recompile sprite sprite_shaders with WIREFRAME flag
set_source_files_properties(${sprite_shaders} PROPERTIES GLSLCC_COMPILE_DEFINITIONS "WIREFRAME" 
                                                         GLSLCC_OUTPUT_FILENAME "sprite_wire")
glslcc_target_compile_shaders_h(2dtools "${sprite_shaders}")

# font shaders
glslcc_target_compile_shaders_h(2dtools "font.vert;font.frag")


