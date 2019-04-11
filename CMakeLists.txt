cmake_minimum_required(VERSION 3.1)
project(rizz)

if (ANDROID OR IOS)
    set(BUNDLE ON CACHE INTERNAL "Bundle mode" FORCE)
    set(BUILD_TOOLS OFF CACHE INTERNAL "Build tools" FORCE)
    set(BUILD_EXAMPLES OFF CACHE INTERNAL "Build examples" FORCE)
    set(ENABLE_HOT_LOADING OFF CACHE INTERNAL "Enable hot-loading" FORCE)
else()
    option(BUNDLE "Bundle all plugins and game together and make a single binary" OFF)
    option(BUILD_TOOLS "Build tools and editors" ON)
    option(BUILD_EXAMPLES "Build example projects" ON)
    option(ENABLE_HOT_LOADING "Enable hot-loading of assets and plugins" ON)
    option(MSVC_COMPILE_SUMMARY "Enable compilation metrics for MSVC compiler" OFF)
endif()

if (${CMAKE_BUILD_TYPE} MATCHES "Release")
    option(ENABLE_PROFILER "Enable profiler" OFF)
else ()
    option(ENABLE_PROFILER "Enable profiler" ON)
endif()

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(glslcc)
include(bundle)

if (NOT CMAKE_RUNTIME_OUTPUT_DIRECTORY)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin)
endif()
if (NOT CMAKE_LIBRARY_OUTPUT_DIRECTORY)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin)
endif()

function(rizz__remove_compile_options DEST_VAR COMPILER_FLAGS FLAGS)
    separate_arguments(FLAGS)
    foreach(FLAG ${FLAGS})
        string(REPLACE "${FLAG}" "" COMPILER_FLAGS "${COMPILER_FLAGS}")
    endforeach()
    set(${DEST_VAR} ${COMPILER_FLAGS} PARENT_SCOPE)
endfunction()

add_definitions(-D__STDC_LIMIT_MACROS)
add_definitions(-D__STDC_FORMAT_MACROS)
add_definitions(-D__STDC_CONSTANT_MACROS)
if(MSVC)
    add_definitions(-D_ITERATOR_DEBUG_LEVEL=0)
    add_definitions(-D_HAS_EXCEPTIONS=0)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS=0)
endif()
if (RPI)
    add_definitions(-D__RPI__)
endif()

if (MSVC)
    rizz__remove_compile_options(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}" "/EHsc /GR")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /GR-")
    if (MSVC_COMPILE_SUMMARY)
        add_compile_options(/d2cgsummary)
    endif()
else()
    add_compile_options(-Wno-deprecated-declarations -fPIC)

    add_compile_options("$<$<CONFIG:Debug>:-D_DEBUG>")
    add_compile_options("$<$<CONFIG:Release>:-DNDEBUG>")

    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu11")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -fno-rtti -fno-exceptions")
    rizz__remove_compile_options(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}" "-fexceptions -frtti")
endif()

get_directory_property(has_parent PARENT_DIRECTORY)

# PLUGIN_TYPE, EXEC_TYPE definitions
# PLUGIN_TYPE: cmake plugin builds should include PLUGIN_TYPE: add_library(plugin ${PLUGIN_TYPE} ...)
# EXEC_TYPE: cmake executable builds should include EXEC_TYPE: add_executable(binary ${EXEC_TYPE} ...)
if (has_parent)
    if (BUNDLE)
        set(PLUGIN_TYPE STATIC PARENT_SCOPE)
    else()
        set(PLUGIN_TYPE SHARED PARENT_SCOPE)
    endif()

    if (WIN32)
        set(EXEC_TYPE WIN32 PARENT_SCOPE)
    endif()
else()
    if (BUNDLE)
        set(PLUGIN_TYPE STATIC)
    else()
        set(PLUGIN_TYPE SHARED)
    endif()

    if (WIN32)
        set(EXEC_TYPE WIN32)
    endif()
endif()

include_directories(include)

# 3rdparty
add_subdirectory(3rdparty/utf)
add_subdirectory(3rdparty/lz4)
add_subdirectory(3rdparty/cimgui)
add_subdirectory(3rdparty/imguizmo)
add_subdirectory(3rdparty/remotery)
if (ENABLE_HOT_LOADING)
    add_subdirectory(3rdparty/efsw)
endif()
if (BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# native projects
set(native_projects sx 
                    rizz
                    imgui
                    sprite)

foreach (native_project ${native_projects})
    add_subdirectory(src/${native_project})
    if (BUNDLE)
        target_compile_definitions(${native_project} PRIVATE -DRIZZ_BUNDLE)
    endif()

    if (MSVC)
        target_compile_options(${native_project} PRIVATE /WX)
    else()
        target_compile_options(${native_project} PRIVATE 
            -Wextra 
            -Wshadow 
            -Wunreachable-code 
            -Wstrict-aliasing 
            -Werror 
            -Wno-missing-field-initializers)
        if (CMAKE_C_COMPILER_ID MATCHES "GNU")
            target_compile_options(${native_project} PRIVATE -Wno-format-truncation)
        endif()
    endif()
endforeach()
