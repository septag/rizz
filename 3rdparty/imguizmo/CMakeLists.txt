Project(cimgui)
cmake_minimum_required(VERSION 3.0)

#general settings
include_directories(imguizmo)
add_definitions("-DIMGUI_DISABLE_OBSOLETE_FUNCTIONS=1")

set(SOURCES ImGuizmo.cpp ImGuizmo.h)

add_library(imguizmo STATIC ${SOURCES})
target_link_libraries(imguizmo PUBLIC cimgui)
target_include_directories(imguizmo PRIVATE ../cimgui/imgui)


