cmake_minimum_required(VERSION 3.1)
project(3dtools)

set(3dtools_sources 3dtools.c 
                    prims3d.c 
                    3dtools-internal.h 
                    ../../include/rizz/3dtools.h
                    README.md)
rizz_add_plugin(3dtools "${3dtools_sources}")

set(GLSLCC_OUTPUT_DIRECTORY "shaders_h")

glslcc_target_compile_shaders_h(3dtools "prims3d.vert;prims3d.frag")