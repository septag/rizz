cmake_minimum_required(VERSION 3.1)
project(basisut)

set(basisut_sources basisut.cpp)
rizz_add_plugin(basisut "${basisut_sources}")

target_link_libraries(basisut PRIVATE basisu)
