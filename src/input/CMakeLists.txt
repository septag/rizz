cmake_minimum_required(VERSION 3.1)
project(input)

add_library(input ${PLUGIN_TYPE} input.cpp
                                 ../../include/rizz/input.h
                                 README.md)

target_link_libraries(input PRIVATE sx gainput)
target_include_directories(input PRIVATE ../../3rdparty)