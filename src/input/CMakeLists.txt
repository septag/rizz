cmake_minimum_required(VERSION 3.1)
project(input)

set(input_sources input.cpp
                  ../../include/rizz/input.h
                  README.md)
rizz_add_plugin(input "${input_sources}")

target_link_libraries(input PRIVATE gainput)
