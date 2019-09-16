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

# output directories for binary files
if (NOT CMAKE_RUNTIME_OUTPUT_DIRECTORY)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin)
endif()
if (NOT CMAKE_LIBRARY_OUTPUT_DIRECTORY)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin)
endif()

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(glslcc)
include(bundle)

function(rizz__remove_compile_options dst_var compile_flags flags)
    separate_arguments(flags)
    foreach(flag ${flags})
        string(REPLACE "${flag}" "" compile_flags "${compile_flags}")
    endforeach()
    set(${dst_var} ${compile_flags} PARENT_SCOPE)
endfunction()

function(rizz__add_example proj_name)
    string(SUBSTRING ${proj_name} 3 -1 source_file)

    if (BUNDLE AND NOT ANDROID) 
        add_executable(${proj_name} ${EXEC_TYPE} examples/${proj_name}/${source_file}.c)
    else()
        add_library(${proj_name} SHARED examples/${proj_name}/${source_file}.c)
    endif()

    if (NOT ANDROID AND NOT IOS)
        if (NOT EXAMPLES_ROOT)
            set(EXAMPLES_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/examples")
        endif()
        target_compile_definitions(${proj_name} PRIVATE COMPILE_DEFINITIONS -DEXAMPLES_ROOT="${EXAMPLES_ROOT}")
    endif()
    
    target_link_libraries(${proj_name} PRIVATE sx)
    add_dependencies(${proj_name} rizz)

    # compile default shaders
    if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/examples/${proj_name}/${source_file}.vert AND 
        EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/examples/${proj_name}/${source_file}.frag)
        set(shaders examples/${proj_name}/${source_file}.vert 
                    examples/${proj_name}/${source_file}.frag)
        set_source_files_properties(${shaders} PROPERTIES GLSLCC_OUTPUT_DIRECTORY "examples/assets/shaders")
        glslcc_target_compile_shaders_sgs(${proj_name} "${shaders}")
    endif()

    if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/examples/${proj_name}/${source_file}.comp)
        set(shaders examples/${proj_name}/${source_file}.comp) 
        set_source_files_properties(${shaders} PROPERTIES GLSLCC_OUTPUT_DIRECTORY "examples/assets/shaders")

		# debug
        set_source_files_properties(${shaders} PROPERTIES GLSLCC_COMPILE_FLAGS "--debug-bin")

        glslcc_target_compile_shaders_sgs(${proj_name} "${shaders}")
    endif()
endfunction()

function(rizz_add_executable proj_name source_files)
    if (BUNDLE AND NOT ANDROID) 
        add_executable(${proj_name} ${EXEC_TYPE} ${source_files})
    else()
        add_library(${proj_name} SHARED ${source_files})
    endif()
    target_link_libraries(${proj_name} PRIVATE sx)
    add_dependencies(${proj_name} rizz)
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

    if (CMAKE_C_COMPILER_ID MATCHES "GNU")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-format-truncation")
    endif()    
endif()

get_directory_property(has_parent PARENT_DIRECTORY)

# PLUGIN_TYPE, EXEC_TYPE definitions
# PLUGIN_TYPE: cmake plugin builds should include PLUGIN_TYPE: add_library(plugin ${PLUGIN_TYPE} ...)
# EXEC_TYPE: cmake executable builds should include EXEC_TYPE: add_executable(binary ${EXEC_TYPE} ...)
if (BUNDLE)
	set(PLUGIN_TYPE STATIC)
else()
	set(PLUGIN_TYPE SHARED)
endif()

if (WIN32)
	set(EXEC_TYPE WIN32)
endif()

# include directory is for all projects
include_directories(include)

# native projects
set(native_projects sx 
                    rizz
                    imgui
                    sprite
                    sound
                    input)

# 3rdparty projects
set(3rdparty_projects utf 
                      lz4
                      cimgui
                      imguizmo
                      remotery
                      efsw
                      gainput)

if (NOT ENABLE_HOT_LOADING)
    list(REMOVE_ITEM 3rdparty_projects efsw)
endif()

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux" AND NOT RPI)
    list(APPEND 3rdparty_projects flextGL)
endif()

# TEMP: gainput rpi support is not included yet
if (RPI)
    list(REMOVE_ITEM 3rdparty_projects gainput)
    list(REMOVE_ITEM native_projects input)
endif()

# examples
set(example_projects 01-hello 
                     02-quad 
                     03-drawsprite 
                     04-animsprite
                     05-playsound
					 06-sdf
					 07-cs)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

foreach (3rdparty_project ${3rdparty_projects}) 
    add_subdirectory(3rdparty/${3rdparty_project})
    set_target_properties(${3rdparty_project} PROPERTIES FOLDER 3rdparty)
endforeach()

foreach (native_project ${native_projects})
    add_subdirectory(src/${native_project})
    if (MSVC)
        target_compile_options(${native_project} PRIVATE /WX)
    else()
        target_compile_options(${native_project} PRIVATE 
            -Wextra 
            -Wshadow 
            -Wunreachable-code 
            -Wstrict-aliasing 
            -Werror 
            -Wno-missing-field-initializers
            -Wno-unused-function)
    endif()

    if (BUNDLE)
        target_compile_definitions(${native_project} PRIVATE -DRIZZ_BUNDLE)
    endif()
endforeach()

if (BUILD_EXAMPLES AND NOT BUNDLE) 
    foreach (example_project ${example_projects})
        rizz__add_example(${example_project})
        set_target_properties(${example_project} PROPERTIES FOLDER examples)
    endforeach()
endif()

# use -DBUNDLE_TARGET=target-name and -DBUNDLE_TARGET_NAME=name to define 
if (BUNDLE)
    if (NOT BUNDLE_TARGET)
        message(FATAL_ERROR "BUNDLE_TARGET is not set, set BUNDLE_TARGET to the target name of your executable target")
    endif()

    if (NOT BUNDLE_TARGET_NAME)
        message(STATUS "BUNDLE_TARGET_NAME is not set. defaults to '${BUNDLE_TARGET}'")
        set(BUNDLE_TARGET_NAME ${BUNDLE_TARGET})
    endif()

    foreach (example_project ${example_projects})
        if ("${BUNDLE_TARGET}" STREQUAL "${example_project}")
            rizz__add_example(${BUNDLE_TARGET})
            break()
        endif()
    endforeach()

    if (BUNDLE_PLUGINS)
        rizz_set_executable(${BUNDLE_TARGET} PLUGINS ${BUNDLE_PLUGINS} NAME ${BUNDLE_TARGET_NAME})
    else()
        rizz_set_executable(${BUNDLE_TARGET} NAME ${BUNDLE_TARGET_NAME})
    endif()
endif()
