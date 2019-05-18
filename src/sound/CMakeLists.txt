cmake_minimum_required(VERSION 3.1)
project(sound)

add_library(sound ${PLUGIN_TYPE} sound.c 
                                 ../../include/rizz/sound.h
                                 ../../3rdparty/sokol/sokol_audio.h
                                 ../../3rdparty/dr_libs/dr_wav.h)
target_link_libraries(sound PRIVATE sx)
target_include_directories(sound PRIVATE ../../3rdparty)
