Project(cimgui)
cmake_minimum_required(VERSION 3.0)

#general settings
include_directories(imgui)
add_definitions("-DIMGUI_DISABLE_OBSOLETE_FUNCTIONS=1")
add_definitions("-DCIMGUI_NO_EXPORT")

include_directories(.)
set(IMGUI_SOURCES cimgui.cpp 
                  imgui/imgui.h 
                  imgui/imgui.cpp 
                  imgui/imgui_draw.cpp 
                  imgui/imgui_widgets.cpp 
                  imgui/imgui_tables.cpp)

add_definitions("-DIMGUI_IMPL_API=extern \"C\" ")

add_library(cimgui STATIC ${IMGUI_SOURCES})

if (MSVC)
    target_compile_options(cimgui PRIVATE /wd4190)
else()
    target_compile_options(cimgui PRIVATE -Wno-return-type-c-linkage)
endif()

