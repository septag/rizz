cmake_minimum_required(VERSION 3.1)
project(3dtools)

set(3dtools_sources 3dtools.c 
                    debug3d.c 
                    model.c 
                    3dtools-internal.h 
                    ../../include/rizz/3dtools.h
                    README.md)
rizz_add_plugin(3dtools "${3dtools_sources}")

set(GLSLCC_OUTPUT_DIRECTORY "shaders_h")

set(debug3d_shaders debug3d.vert debug3d.frag)
glslcc_target_compile_shaders_h(3dtools "${debug3d_shaders}")
set_source_files_properties(${debug3d_shaders} PROPERTIES GLSLCC_COMPILE_DEFINITIONS "BOX_UV_WORKAROUND" 
                                                          GLSLCC_OUTPUT_FILENAME "debug3d_box")

glslcc_target_compile_shaders_h(3dtools "${debug3d_shaders}")
glslcc_target_compile_shaders_h(3dtools "debug3d_wire.vert;debug3d_wire.frag")