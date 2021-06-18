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
endif()

if (${CMAKE_BUILD_TYPE} MATCHES "Release")
    option(ENABLE_PROFILER "Enable profiler" OFF)
else ()
    option(ENABLE_PROFILER "Enable profiler" ON)
endif()

# Clang-Cl detection
if (CMAKE_C_COMPILER_ID MATCHES "Clang" AND MSVC)
    set(CLANG_CL 1 CACHE INTERNAL BOOl "")
    message(STATUS "using clang-cl compiler")
endif()

if (MSVC)
    option(MSVC_STATIC_RUNTIME "Link with MSVC static runtime. Only sets for 'Release' build" OFF)
    option(MSVC_MULTITHREADED_COMPILE "Multi-threaded compilation in MSVC." ON)
endif()

if (CMAKE_C_COMPILER_ID MATCHES "Clang")
    option(CLANG_ENABLE_PROFILER "Enable clang build profiler ('-ftime-trace') flag" OFF)
endif()

# set MACOSX_BUNDLE_ROOT_DIR to define the path that cmake can find resource/plist files
if (NOT MACOSX_BUNDLE_ROOT_DIR)
    set (MACOSX_BUNDLE_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR})
endif()

if (NOT MSVC)
    set(CMAKE_CXX_STANDARD 14)
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
include(ispc)

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
        add_executable(${proj_name} examples/${proj_name}/${source_file}.c)
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

	IF (MSVC)
		set_target_properties(${proj_name} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:${proj_name}>/")
		set_target_properties(${proj_name} PROPERTIES VS_DEBUGGER_COMMAND "rizz.exe")
		set_target_properties(${proj_name} PROPERTIES VS_DEBUGGER_COMMAND_ARGUMENTS "--run ${proj_name}.dll")
        set_target_properties(${proj_name} PROPERTIES LINK_FLAGS "/INCREMENTAL:NO")
	ENDIF()

    # compile default shaders
    set(GLSLCC_SOURCE_GROUP "shaders")
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
        glslcc_target_compile_shaders_sgs(${proj_name} "${shaders}")
    endif()

    if (BUNDLE)
        # set bundle properties
        if (NOT BUNDLE_TARGET_NAME)
            message(STATUS "BUNDLE_TARGET_NAME is not set. defaults to '${proj_name}'")
            set(BUNDLE_TARGET_NAME ${proj_name})
        endif()

        if (BUNDLE_PLUGINS)
            rizz_set_executable(${proj_name} PLUGINS ${BUNDLE_PLUGINS} NAME ${BUNDLE_TARGET_NAME})
        else()
            rizz_set_executable(${proj_name} NAME ${BUNDLE_TARGET_NAME})
        endif()    

        # set macosx/ios properties
        if (APPLE) 
            set(resource_files ${MACOSX_BUNDLE_ROOT_DIR}/assets 
                               ${MACOSX_BUNDLE_ROOT_DIR}/images.xcassets)
            target_sources(${proj_name} PRIVATE "${resource_files}")
            set_target_properties(${proj_name} PROPERTIES 
                MACOSX_BUNDLE TRUE 
                MACOSX_BUNDLE_INFO_PLIST ${MACOSX_BUNDLE_ROOT_DIR}/Info.plist 
                XCODE_ATTRIBUTE_ASSETCATALOG_COMPILER_APPICON_NAME "AppIcon" 
                XCODE_ATTRIBUTE_ASSETCATALOG_COMPILER_LAUNCHIMAGE_NAME "LaunchImage" 
                RESOURCE "${resource_files}")
        endif()
    endif()

    if (WIN32)
        set_target_properties(${proj_name} PROPERTIES WIN32_EXECUTABLE TRUE)
    endif()    
endfunction()

function(rizz_add_executable proj_name source_files)
    if (BUNDLE AND NOT ANDROID) 
        add_executable(${proj_name} ${source_files})
    else()
        add_library(${proj_name} SHARED ${source_files})
    endif()
    target_link_libraries(${proj_name} PRIVATE sx)
    add_dependencies(${proj_name} rizz)

    # set bundle properties
    if (BUNDLE)
        if (NOT BUNDLE_TARGET_NAME)
            message(STATUS "BUNDLE_TARGET_NAME is not set. defaults to '${proj_name}'")
            set(BUNDLE_TARGET_NAME ${proj_name})
        endif()

        if (BUNDLE_PLUGINS)
            rizz_set_executable(${proj_name} PLUGINS ${BUNDLE_PLUGINS} NAME ${BUNDLE_TARGET_NAME})
        else()
            rizz_set_executable(${proj_name} NAME ${BUNDLE_TARGET_NAME})
        endif()    

        # set macosx/ios properties
        if (APPLE) 
            set(resource_files ${MACOSX_BUNDLE_ROOT_DIR}/assets 
                               ${MACOSX_BUNDLE_ROOT_DIR}/images.xcassets)
            target_sources(${proj_name} PRIVATE "${resource_files}")
            set_target_properties(${proj_name} PROPERTIES 
                MACOSX_BUNDLE TRUE 
                MACOSX_BUNDLE_INFO_PLIST ${MACOSX_BUNDLE_ROOT_DIR}/Info.plist 
                XCODE_ATTRIBUTE_ASSETCATALOG_COMPILER_APPICON_NAME "AppIcon" 
                XCODE_ATTRIBUTE_ASSETCATALOG_COMPILER_LAUNCHIMAGE_NAME "LaunchImage" 
                RESOURCE "${resource_files}")
        endif()
    endif()

    if (WIN32)
        set_target_properties(${proj_name} PROPERTIES WIN32_EXECUTABLE TRUE)
        set_target_properties(${proj_name} PROPERTIES LINK_FLAGS "/INCREMENTAL:NO")
    endif()
endfunction()

function(rizz_add_plugin proj_name source_files)
    if (BUNDLE)
        add_library(${proj_name} STATIC ${source_files})
    else()
        add_library(${proj_name} SHARED ${source_files})
    endif()

    target_link_libraries(${proj_name} PRIVATE sx)
    target_include_directories(${proj_name} PRIVATE ../../3rdparty)
    add_dependencies(${proj_name} rizz)
    set_target_properties(${proj_name} PROPERTIES FOLDER plugins)
endfunction()

add_definitions(-D__STDC_LIMIT_MACROS)
add_definitions(-D__STDC_FORMAT_MACROS)
add_definitions(-D__STDC_CONSTANT_MACROS)
if(WIN32)
    add_definitions(-D_ITERATOR_DEBUG_LEVEL=0)
    add_definitions(-D_SECURE_SCL=0)
    add_definitions(-D_HAS_EXCEPTIONS=0)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS=0)
endif()
if (RPI)
    add_definitions(-D__RPI__)
endif()

if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION")
    file(STRINGS "VERSION" version)
    message(STATUS "Build version: ${version}")
endif()

macro(rizz_set_compile_flags_current_dir)
if (MSVC)
    rizz__remove_compile_options(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}" "/EHsc /GR")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /GR-")
    if (MSVC_MULTITHREADED_COMPILE AND NOT CMAKE_MAKE_PROGRAM MATCHES "ninja.exe")
        add_compile_options(/MP)    # multiprocessor compilation
    endif()

    if (MSVC_STATIC_RUNTIME) 
        rizz__remove_compile_options(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}" "/MD")
        rizz__remove_compile_options(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}" "/MD")
		set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT")
		set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} /MT")

        rizz__remove_compile_options(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}" "/MDd")
        rizz__remove_compile_options(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}" "/MDd")
		set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MTd")
		set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} /MTd")
    endif()

    if (${MSVC_VERSION} GREATER_EQUAL 1928)
        message(STATUS "MSVC compiler supports C17 standard")
        # add_compile_options(/std:c17)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /std:c17")
    endif()

    rizz__remove_compile_options(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}" "/Ob0")
    rizz__remove_compile_options(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}" "/Ob0")
	set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /Ob1")
	set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} /Ob1")
else()
    if (NOT WIN32)
        add_compile_options(-fPIC)
    endif()
    add_compile_options(-Wno-deprecated-declarations)
    add_compile_options("$<$<CONFIG:Debug>:-D_DEBUG>")
    add_compile_options("$<$<CONFIG:Release>:-DNDEBUG>")

    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu11")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -fno-rtti -fno-exceptions")
    rizz__remove_compile_options(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}" "-fexceptions -frtti")

    if (CMAKE_C_COMPILER_ID MATCHES "GNU")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-format-truncation")
    endif()
endif()

if (CLANG_ENABLE_PROFILER)
    string(FIND ${CMAKE_C_COMPILER_VERSION} "." major_ver_end)
    if (${major_ver_end} GREATER 0)
        string(SUBSTRING ${CMAKE_C_COMPILER_VERSION} 0 ${major_ver_end} clang_major_version)
        if (${clang_major_version} GREATER_EQUAL 9)
            # https://aras-p.info/blog/2019/01/16/time-trace-timeline-flame-chart-profiler-for-Clang/
            # https://github.com/aras-p/ClangBuildAnalyzer
            message(STATUS "Enabling clang built-in profiler: -ftime-trace")
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS_RELEASE} -ftime-trace")
            set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS_RELEASE} -ftime-trace")            
        else()
            message(WARNING "Current clang version does not support built-in profiler")
        endif()
    endif()
endif()
endmacro()

rizz_set_compile_flags_current_dir()

# clang-cl complains about returning structs from c-extern return type, which is valid, but we don't need them now
if (CLANG_CL)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-return-type-c-linkage")
endif()

get_directory_property(has_parent PARENT_DIRECTORY)

# include directory is for all projects
include_directories(include)

# native projects + installed plugins
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/plugins")
    file(STRINGS "plugins" installed_plugins)
endif()

set(native_projects sx 
                    rizz 
                    imgui
                    2dtools
                    3dtools 
                    astar 
                    utility 
                    sound
                    input
                    collision)

if (installed_plugins)
    message(STATUS "The following plugins are found:")
    foreach (plugin ${installed_plugins})
        message(STATUS "    ${plugin}")
    endforeach()
    list(APPEND native_projects ${installed_plugins})
endif()

# 3rdparty projects
set(3rdparty_projects cimgui
                      imguizmo
                      remotery
                      gainput
                      basisu)

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
                     07-nbody
                     08-draw3d
                     09-pathfind
                     10-collideboxes
                     11-pathspline
                     12-boids)
                     
# exceptions
# currently compute-shaders are only supported on windows, so we ignore examples that use them
if (NOT WIN32) 
    list(REMOVE_ITEM example_projects 07-nbody)
endif()

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

foreach (3rdparty_project ${3rdparty_projects}) 
    add_subdirectory(3rdparty/${3rdparty_project})
    set_target_properties(${3rdparty_project} PROPERTIES FOLDER 3rdparty)
    if (MSVC)
        set_target_properties(${3rdparty_project} PROPERTIES LINK_FLAGS "/INCREMENTAL:NO")
    endif()
endforeach()

foreach (native_project ${native_projects})
    add_subdirectory(src/${native_project})
    if (MSVC)
        if (CMAKE_MAKE_PROGRAM MATCHES "ninja.exe")
            target_compile_options(${native_project} PRIVATE /WX /W3)
        else()
            target_compile_options(${native_project} PRIVATE /WX /W4)
            target_compile_options(${native_project} PRIVATE 
                /wd4201  # nonstandard extension used : nameless struct/union
                /wd4324  # structure was padded due to alignment specifier
                /wd4206  # nonstandard extension used: translation unit is empty
                /wd4214  # nonstandard extension used: bit field types other than int
                /wd4221  # cannot be initialized using address of automatic variable
                /wd4053  # one void operand for
                /wd4204  # nonstandard extension used: non-constant aggregate initializer
                /wd4127  # conditional expression is constant (if (1)) 
                ) 
        endif()
        set_target_properties(${native_project} PROPERTIES LINK_FLAGS "/INCREMENTAL:NO")
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

    target_compile_definitions(${native_project} PUBLIC "$<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:RIZZ_DEV_BUILD>")
endforeach()

if (BUILD_EXAMPLES AND NOT BUNDLE) 
    foreach (example_project ${example_projects})
        rizz__add_example(${example_project})
        set_target_properties(${example_project} PROPERTIES FOLDER examples)
    endforeach()
endif()

# search in examples for BUNDLE_TARGET, if matched, build it as a bundle
# use -DBUNDLE=1, -DBUNDLE_TARGET=example-project and -DBUNDLE_TARGET_NAME=example-name
# example: cmake -DBUNDLE=1 -DBUNDLE_TARGET=01-hello -DBUNDLE_TARGET_NAME=hello
if (BUNDLE AND BUNDLE_TARGET)
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
endif()
