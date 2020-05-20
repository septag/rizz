cmake_minimum_required(VERSION 3.1)
project(3dtools)

set(3dtools_sources 3dtools.c 
                    prims3d.c 
                    3dtools-internal.h 
                    ../../include/rizz/3dtools.h
                    README.md)
rizz_add_plugin(3dtools "${3dtools_sources}")
