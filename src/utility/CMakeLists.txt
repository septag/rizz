cmake_minimum_required(VERSION 3.1)
project(utility)

set(utility_sources utility.c 
                  spline.c 
                  noise.c 
                  ../../include/rizz/spline.h
                  ../../include/rizz/noise.h
                  README.md)
rizz_add_plugin(utility "${utility_sources}")