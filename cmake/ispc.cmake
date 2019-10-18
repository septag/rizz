# ispc.cmake: helper for integrating ispc compilation into cmake projects
# Copyright 2019 Sepehr Taghdisian. All rights reserved. 
# https://github.com/septag/rizz
#
# Functions:
#   ispc_target_add_files(target source_files):
#       adds files to specified target
#       each source file can have properties mentioned below
# 
# Variables:
#   ISPC_PATH: Path to ispc compiler on the system. If not provided, the module tries to find it in 
#              the system path automatically
# The following variables can also be set as source file properties using `set_source_files_properties` function
#   ISPC_OUTPUT_DIRECTORY: output directory for files. 
#   ISPC_COMPILE_DEFINITIONS: set global compile definitions. seperated with semicolon. 
#   ISPC_INCLUDE_DIRS: include directories. seperated with semicolon.
#   ISPC_COMPILE_FLAGS: Extra compilation flags
#   ISPC_OUTPUT_FILENAME: output filename (default is source_file.ispc.o). can only be used as source file property
#   ISPC_SOURCE_GROUP: source group used in visual-studio and xcode IDEs
#   ISPC_TARGET_ARCH: Target cpu architecture. can be 'arm', 'aarch64', 'x86' or 'x86-64'. 
#                     If not provided, the value will be set automatically based on the build system:
#                     windows/linux/OSX: x86-64
#                     android/iOS: aarch64 (arm64), arm (arm), x86(simulator), x86-64 (simulator64)
#                     raspberryPI: arm
#   ISPC_TARGET_SIMD: Define SIMD target and lanes. can be sse2-i32x4, sse2-i32x8, sse4-i32x4, sse4-i32x8, sse4-i16x8, sse4-i8x16, avx1-i32x4, avx1-i32x8, avx1-i32x16, avx1-i64x4, avx2-i32x4, avx2-i32x8, avx2-i32x16, avx2-i64x4, avx512knl-i32x16, avx512skx-i32x16, avx512skx-i32x8, generic-x1, generic-x4, generic-x8, generic-x16, generic-x32, generic-x64, *-generic-x16, neon-i8x16, neon-i16x8, neon-i32x4, neon-i32x8
#                     If not provided, the value will be set automatically based on the build system:
#                     windows/linux/OSX: avx1-i32x8
#                     android/rpi/iOS: neon-i32x4
#                   
string(TOLOWER ${CMAKE_SYSTEM_NAME} ipsc_platform_name)
if (IOS)
    set(ipsc_platform_name "ios")
elseif (ANDROID)
    set(ipsc_platform_name "android")
elseif (RPI)
    set(ipsc_platform_name "rpi")
endif()

# resolve ispc binary
if (NOT ISPC_PATH AND NOT ispc_bin_filepath)
    # try to find ispc executable path
    # also take a guess (most likely for personal projects)
    if (${CMAKE_HOST_SYSTEM_NAME} STREQUAL "Windows")
        set(tools_dir "${PROJECT_SOURCE_DIR}/tools/windows")    
    elseif (${CMAKE_HOST_SYSTEM_NAME} STREQUAL "Darwin")
        set(tools_dir "${PROJECT_SOURCE_DIR}/tools/darwin")
    elseif(${CMAKE_HOST_SYSTEM_NAME} STREQUAL "Linux")
        set(tools_dir "${PROJECT_SOURCE_DIR}/tools/linux")
    endif()

    if (IOS)
        set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
    endif()

    find_program(ispc_bin_filepath 
                NAMES ispc
                PATHS ${tools_dir} ENV PATH
                DOC "path to ispc binary")

    if (IOS)
        set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM ONLY)
    endif()

    if (ispc_bin_filepath)
        message(STATUS "Found ispc: ${ispc_bin_filepath}")
    endif()
else()
    if (EXISTS ${ISPC_PATH})
        set(ispc_bin_filepath ${ISPC_PATH} CACHE INTERNAL "path to ispc binary")
        message(STATUS "Found ispc: ${ispc_bin_filepath}")
    endif()
endif()

# properties
define_property(SOURCE PROPERTY ISPC_OUTPUT_DIRECTORY 
                BRIEF_DOCS "Compiled ispc output directory"
                FULL_DOCS "Compiled ispc output directory")
define_property(SOURCE PROPERTY ISPC_COMPILE_DEFINITIONS 
                BRIEF_DOCS "Compiler definitions, seperated by semicolon"
                FULL_DOCS "Compiler definitions, seperated by semicolon")                
define_property(SOURCE PROPERTY ISPC_INCLUDE_DIRS 
                BRIEF_DOCS "ispc include directories"
                FULL_DOCS "ispc include directories, seperated with semicolon")  
define_property(SOURCE PROPERTY ISPC_COMPILE_FLAGS 
                BRIEF_DOCS "Extra compile flags"
                FULL_DOCS "Extra compile flags")
define_property(SOURCE PROPERTY ISPC_OUTPUT_FILENAME
                BRIEF_DOCS "Compiled ispc output filename"
                FULL_DOCS "Compiled ispc output filename. Default is SOURCE_FILE.o")
define_property(SOURCE PROPERTY ISPC_SOURCE_GROUP 
                BRIEF_DOCS "ispc Source group"
                FULL_DOCS "ispc Source group, child subdirectories will be created for object files")
define_property(SOURCE PROPERTY ISPC_TARGET_ARCH
                BRIEF_DOCS "target architecture (arm, aarch64, x86, x86-64)"
                FULL_DOCS "target architecture (arm, aarch64, x86, x86-64), will be switched to default for each platform if not set")
define_property(SOURCE PROPERTY ISPC_TARGET_SIMD
                BRIEF_DOCS "SIMD mode (sse2-i32x4, sse4-i32x4, avx1-i32x8, neon-i32x4, ...)"
                FULL_DOCS "SIMD mode (sse2-i32x4, sse4-i32x4, avx1-i32x8, neon-i32x4, ...), see ispc --target flags")

function(ispc_target_add_files target_name source_files)
    foreach(source_file ${source_files})
        get_source_file_property(defs ${source_file} ISPC_COMPILE_DEFINITIONS)
        get_source_file_property(include_dirs ${source_file} ISPC_INCLUDE_DIRS)
        get_source_file_property(output_dir ${source_file} ISPC_OUTPUT_DIRECTORY)
        get_source_file_property(compile_flags ${source_file} ISPC_COMPILE_FLAGS)
        get_source_file_property(output_filename ${source_file} ISPC_OUTPUT_FILENAME)
        get_source_file_property(source_group_name ${source_file} ISPC_SOURCE_GROUP)
        get_source_file_property(target_arch ${source_file} ISPC_TARGET_ARCH)
        get_source_file_property(target_simd ${source_file} ISPC_TARGET_SIMD)

        if (ISPC_COMPILE_DEFINITIONS)
            set(defs ${defs} ${ISPC_COMPILE_DEFINITIONS})
        endif()
        if (ISPC_INCLUDE_DIRS)
            set(include_dirs ${include_dirs} ${ISPC_INCLUDE_DIRS})
        endif()
        if (NOT output_dir)
            set(output_dir ${ISPC_OUTPUT_DIRECTORY})
        endif()
        if (ISPC_COMPILE_FLAGS)
            set(compile_flags ${compile_flags} ${ISPC_COMPILE_FLAGS})
        endif()
        if (NOT source_group_name)
            set(source_group_name ${ISPC_SOURCE_GROUP})
        endif()
        if (NOT target_arch)
            set(target_arch ${ISPC_TARGET_ARCH})
        endif()
        if (NOT target_simd)
            set(target_simd ${ISPC_TARGET_SIMD})
        endif()

        if (include_dirs)
            set(arg_includes)
            foreach (include_dir ${include_dirs})
                if (IS_DIRECTORY ${include_dir})
                    get_filename_component(include_dir ${include_dir} ABSOLUTE)
                    list(APPEND arg_includes -I ${include_dir})                    
                else()
                    message(WARNING "invalid directory: ${include_dir}")
                endif()
            endforeach()
        endif()

        if (defs)
            set(arg_defs)
            foreach (def ${defs})
                list(APPEND arg_defs -D${def})
            endforeach()
        endif()

        if (NOT output_dir)
            get_filename_component(output_dir ${source_file} DIRECTORY)
            if (NOT output_dir)
                set(output_dir ".")
            endif()
            get_filename_component(output_dir ${output_dir} ABSOLUTE)
        endif()

        if (output_dir)
            # append system_name to output_path
            if (NOT IS_DIRECTORY ${output_dir})
                file(MAKE_DIRECTORY ${output_dir})
            endif()
            get_filename_component(output_dir ${output_dir} ABSOLUTE)
        else()            
            message(FATAL_ERROR "ISPC_OUTPUT_DIRECTORY not set for file: ${source_file}")
        endif()

        if (${output_filename} STREQUAL "NOTFOUND")
            set(output_filename "${source_file}.o")
        endif()
        set(output_filepath "${output_dir}/${output_filename}")

        # arch: if not defined, set default for each platform
        if (NOT target_arch)
            if (${glslcc_platform_name} STREQUAL "windows")
                set(target_arch "x86-64")
            elseif (${glslcc_platform_name} STREQUAL "android")
                if (ANDROID_ABI STREQUAL armeabi-v7a)
                    set(target_arch "arm")
                elseif (ANDROID_ABI STREQUAL arm64-v8a)
                    set(target_arch "aarch64")
                elseif (ANDROID_ABI STREQUAL x86)
                    set(target_arch "x86")
                elseif (ANDROID_ABI STREQUAL x86_64)
                    set(target_arch "x86-64")
                endif()
            elseif (${glslcc_platform_name} STREQUAL "rpi")
                set(target_arch "arm")                
            elseif (${glslcc_platform_name} STREQUAL "ios")
                if (IOS_ARCH STREQUAL armv7 OR IOS_ARCH STREQUAL armv7s)
                    set(target_arch "arm")
                elseif (IOS_ARCH STREQUAL arm64)
                    set(target_arch "aarch64")
                elseif (IOS_ARCH STREQUAL i386)
                    set(target_arch "x86")
                elseif (IOS_ARCH STREQUAL x86_64)
                    set(target_arch "x86-64")
                endif()
            elseif (${glslcc_platform_name} STREQUAL "linux")
                set(target_arch "x86-64")
            elseif (${glslcc_platform_name} STREQUAL "darwin")
                set(target_arch "x86-64")
            else()
                message(FATAL_ERROR "target architecture is not supported on this platform")
            endif()
        endif()

        # SIMD ISA and width: set defaults for each platform if not set by user
        if (NOT target_simd)
            if (${glslcc_platform_name} STREQUAL "windows" OR ${glslcc_platform_name} STREQUAL "linux" OR ${glslcc_platform_name} STREQUAL "darwin")
                set(target_simd "avx1-i32x8")
            elseif (${glslcc_platform_name} STREQUAL "android" OR ${glslcc_platform_name} STREQUAL "ios")
                if (${target_arch} STREQUAL "x86-64" OR ${target_arch} STREQUAL "x86")
                    set(target_simd "sse4-i32x4")
                else()
                    set(target_simd "neon-i32x4")
                endif()
            elseif (${glslcc_platform_name} STREQUAL "rpi")
                set(target_simd "neon-i32x4")                
            else()
                message(FATAL_ERROR "target architecture is not supported on this platform")
            endif()            
        endif()

        # make final arguments
        set(args ${source_file})

        if (arg_defs)
            set(args ${args} ${arg_defs})
        endif()

        if (arg_includes)
            set(args ${args} ${arg_includes})
        endif()

        if (compile_flags)
            set(args ${args} ${compile_flags})
        endif()

        set(args ${args} --arch=${target_arch})
        set(args ${args} --target=${target_simd})
        set(args ${args} $<IF:$<CONFIG:DEBUG>,-g,-O2>)
        set(args ${args} -o ${output_filepath})

        file(RELATIVE_PATH output_relpath ${CMAKE_CURRENT_SOURCE_DIR} ${output_filepath})
        add_custom_command(
            OUTPUT ${output_filepath}
            COMMAND ${ispc_bin_filepath}
            ARGS ${args}
            DEPENDS ${source_file}
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            COMMENT "Compiling ispc: ${output_relpath}"
            VERBATIM)
        
        # add to srouce files
        target_sources(${target_name} PRIVATE ${source_file} ${output_relpath})

        if (source_group_name)
            source_group(${source_group_name} FILES ${source_file})
            source_group(${source_group_name}\\${obj} FILES ${output_relpath})
        endif()
    endforeach()
endfunction()