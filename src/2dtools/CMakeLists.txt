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

set(shaders sprite.vert sprite.frag)
set_source_files_properties(${shaders} PROPERTIES GLSLCC_OUTPUT_DIRECTORY "shaders_h")
glslcc_target_compile_shaders_h(2dtools "${shaders}")

# recompile sprite shaders with WIREFRAME flag
set_source_files_properties(${shaders} PROPERTIES GLSLCC_COMPILE_DEFINITIONS "WIREFRAME" 
                                                  GLSLCC_OUTPUT_FILENAME "sprite_wire")
glslcc_target_compile_shaders_h(2dtools "${shaders}")