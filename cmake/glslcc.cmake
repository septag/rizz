# glslcc.cmake: helper for integrating glslcc compilation into cmake projects
# Copyright 2019 Sepehr Taghdisian. All rights reserved. 
# https://github.com/septag/glslcc
#
# Functions:
#   glslcc_target_compile_shaders(target source_files):
#       compile shaders and add them to the target. it will generate normal shaders (glsl/hlsl/msl files + reflection.json)
#   glslcc_target_compile_shaders_h(target source_files):
#       compile shaders as C header and add them to the target
#   glslcc_target_compile_shaders_sgs(target source_files):
#       compile shaders as SGS file. SGS files are binary files that integrate multiple shaders with their reflection info    
#       This function packs multiple input files into single SGS file
#       for example: glslcc_target_compile_shaders_sgs(target myshader.vert myshader.frag) will build a single myshader.sgs file
#
# Variables:
#   GLSLCC_BIN: Path to glslcc compiler on the system. If not provided, the module tries to find it in 
#               the system path automatically
# The following variables can also be set as source file properties using `set_source_files_properties` function
#   GLSLCC_OUTPUT_DIRECTORY: output directory for files. 
#   GLSLCC_COMPILE_DEFINITIONS: set global compile definitions. seperated with semicolon. 
#   GLSLCC_INCLUDE_DIRS: include directories. seperated with semicolon.
#   GLSLCC_COMPILE_FLAGS: Extra compilation flags
#   GLSLCC_OUTPUT_FILENAME: output filename (default is source_file.shader_lang.h). can only be used as source file property
#   GLSLCC_SOURCE_GROUP: source group used in visual-studio and xcode IDEs
#   GLSLCC_SHADER_LANG: shader language. if not provided, the value wil be selected automatically:
#                       windows: hlsl
#                       android/rpi: gles
#                       iOS/MacOS: msl (metal)
#                       linux: glsl
#   GLSLCC_SHADER_VERSION: shader version. if not provided, the value will be selected automatically: 
#                       windows: 50 (HLSL 5.0)
#                       android: 300 (OpenGL-ES 3.0)
#                       rpi: 200 (OpenGL-ES 2.0)
#                       linux: 330 (OpenGL 3.3)
#

# determine compilation platform
string(TOLOWER ${CMAKE_SYSTEM_NAME} glslcc_platform_name)
if (IOS)
    set(glslcc_platform_name "ios")
elseif (ANDROID)
    set(glslcc_platform_name "android")
elseif (RPI)
    set(glslcc_platform_name "rpi")
endif()

# resolve glslcc binary
if (NOT GLSLCC_BIN AND NOT glslcc_path)
    # try to find glslcc executable path
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

    find_program(glslcc_path 
                NAMES glslcc
                PATHS ${tools_dir} ENV PATH
                DOC "path to glslcc binary")

    if (IOS)
        set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM ONLY)
    endif()

    if (glslcc_path)
        message(STATUS "Found glslcc: ${glslcc_path}")
    endif()
else()
    if (EXISTS ${GLSLCC_BIN})
        set(glslcc_path ${GLSLCC_BIN} CACHE INTERNAL "path to glslcc binary")
        message(STATUS "Found glslcc: ${glslcc_path}")
    endif()
endif()

if (NOT EXISTS ${glslcc_path})
    message(FATAL_ERROR "glslcc binary '${glslcc_path}' does not exist")
endif()

# PROPERTY: output directory (mandatory)
define_property(SOURCE PROPERTY GLSLCC_OUTPUT_DIRECTORY 
                BRIEF_DOCS "Compiled shader output directory"
                FULL_DOCS "Compiled shader output directory")
# PROPERTY: include direcotries
define_property(SOURCE PROPERTY GLSLCC_INCLUDE_DIRS 
                BRIEF_DOCS "Compiled shader include directories"
                FULL_DOCS "Compiled shader include directories, seperated with comma")   
# PROPERTY: compile definitions
define_property(SOURCE PROPERTY GLSLCC_COMPILE_DEFINITIONS 
                BRIEF_DOCS "Compiled shader compile definitions"
                FULL_DOCS "Compiled shader output directory, seperated with comma")                   

# PROPERTY:  shader language (optional)
define_property(SOURCE PROPERTY GLSLCC_SHADER_LANG 
                BRIEF_DOCS "Target language"
                FULL_DOCS "Target language (gles/msl/hlsl/glsl)")
# PROPERTY: shader_version (optional)
define_property(SOURCE PROPERTY GLSLCC_SHADER_VERSION 
                BRIEF_DOCS "Shader profile version"
                FULL_DOCS "Shader profile version: (HLSL: 40, 50, 60), (ES: 200, 300), (GLSL: 330, 400, 420)")                
# PROPERTY: shader_output_file (optional)
define_property(SOURCE PROPERTY GLSLCC_OUTPUT_FILENAME
                BRIEF_DOCS "Compiled shader output filename"
                FULL_DOCS "Compiled shader output filename. Default is SOURCE_FILE.EXT")
# PROPERTY: extra flags (optional)
define_property(SOURCE PROPERTY GLSLCC_COMPILE_FLAGS 
                BRIEF_DOCS "Extra compile flags"
                FULL_DOCS "Extra compile flags")
# PROPERTY: 
define_property(SOURCE PROPERTY GLSLCC_SOURCE_GROUP 
                BRIEF_DOCS "Shader Source group"
                FULL_DOCS "Shader Source group, child subdirectories will be created for each output type")

function(join_array output glue values)
    string (REGEX REPLACE "([^\\]|^);" "\\1${glue}" tmp_str "${values}")
    string (REGEX REPLACE "[\\](.)" "\\1" tmp_str "${tmp_str}") #fixes escaping
    set (${output} "${tmp_str}" PARENT_SCOPE)
endfunction()        

function(glslcc__target_compile_shaders target_name file_type source_files)
    foreach(source_file_base ${source_files})
        string(REPLACE "," ";" source_files_sub ${source_file_base})

        # get file properties
        list(GET source_files_sub 0 first_file)
        list(LENGTH source_files_sub source_files_count)

        if (${source_files_count} GREATER 1 AND NOT ${file_type} STREQUAL "sgs")
            message(FATAL_ERROR "only 'sgs' shader types support multiple shader stages")
        endif()

        get_source_file_property(defs ${first_file} GLSLCC_COMPILE_DEFINITIONS) 
        get_source_file_property(include_dirs ${first_file} GLSLCC_INCLUDE_DIRS) 
        get_source_file_property(output_dir ${first_file} GLSLCC_OUTPUT_DIRECTORY)
        get_source_file_property(shader_lang ${first_file} GLSLCC_SHADER_LANG)
        get_source_file_property(shader_ver ${first_file} GLSLCC_SHADER_VERSION)
        get_source_file_property(output_filename ${first_file} GLSLCC_OUTPUT_FILENAME)
        get_source_file_property(compile_flags ${first_file} GLSLCC_COMPILE_FLAGS)
        get_source_file_property(source_group_name ${first_file} GLSLCC_SOURCE_GROUP)
        
        if (GLSLCC_COMPILE_DEFINITIONS)
            set(defs ${defs} ${GLSLCC_COMPILE_DEFINITIONS})
        endif()
        if (GLSLCC_INCLUDE_DIRS)
            set(include_dirs ${include_dirs} ${GLSLCC_INCLUDE_DIRS})
        endif()
        if (NOT output_dir)
            set(output_dir ${GLSLCC_OUTPUT_DIRECTORY})
        endif()
        if (NOT shader_lang)
            set(shader_lang ${GLSLCC_SHADER_LANG})
        endif()
        if (NOT shader_ver)
            set(shader_ver ${GLSLSCC_SHADER_VERSION})
        endif()
        if (GLSLCC_COMPILE_FLAGS)
            set(compile_flags ${compile_flags} ${GLSLCC_COMPILE_FLAGS})
        endif()
        if (NOT source_group_name)
            set(source_group_name ${GLSLCC_SOURCE_GROUP})
        endif()

        if (include_dirs)
            set(include_dirs_abs)
            foreach(include_dir ${include_dirs})
                if (IS_DIRECTORY ${include_dir})
                    get_filename_component(include_dir ${include_dir} ABSOLUTE)
                    list(APPEND include_dirs_abs ${include_dir})
                else()
                    message(WARNING "invalid directory: ${include_dir}")
                endif()
            endforeach()
            join_array(include_dirs_abs $<SEMICOLON> "${include_dirs_abs}")
        endif()

        if (defs)
            join_array(defs $<SEMICOLON> "${defs}")
        endif()

        # default shader languages
        # windows: hlsl (50)
        # linux: glsl (330)
        # android: gles (200)
        # ios, osx: msl
        if (NOT shader_lang)
            if (${glslcc_platform_name} STREQUAL "windows")
                set(shader_lang "hlsl")
            elseif (${glslcc_platform_name} STREQUAL "android")
                set(shader_lang "gles")
            elseif (${glslcc_platform_name} STREQUAL "rpi")
                set(shader_lang "gles")                
            elseif (${glslcc_platform_name} STREQUAL "ios")
                set(shader_lang "msl")
            elseif (${glslcc_platform_name} STREQUAL "linux")
                set(shader_lang "glsl")
            elseif (${glslcc_platform_name} STREQUAL "darwin")
                set(shader_lang "msl")
            else()
                message(FATAL_ERROR "shader language is not supported on this platform")
            endif()
        endif()

        if (NOT shader_ver)
            if (${glslcc_platform_name} STREQUAL "windows")
                set(shader_ver "50")
            elseif (${glslcc_platform_name} STREQUAL "android")
                set(shader_ver "300")
            elseif (${glslcc_platform_name} STREQUAL "rpi")
                set(shader_ver "200")                
            elseif (${glslcc_platform_name} STREQUAL "linux")
                set(shader_ver "330")
            endif()
        endif()

        if (NOT output_dir)
            get_filename_component(output_dir ${first_file} DIRECTORY)
            if (NOT output_dir)
                set(output_dir ".")
            endif()            
            get_filename_component(output_dir ${output_dir} ABSOLUTE)
        endif()

        if (output_dir)
            # append system_name to output_path
            set(output_dir ${output_dir}/${shader_lang})

            if (${shader_lang} STREQUAL "gles")
                if (${shader_ver} STREQUAL "200")
                    set(output_dir ${output_dir}2)
                elseif ( ${shader_ver} STREQUAL "300")
                    set(output_dir ${output_dir}3)
                endif()
            endif()

            if (NOT IS_DIRECTORY ${output_dir})
                file(MAKE_DIRECTORY ${output_dir})
            endif()
            get_filename_component(output_dir ${output_dir} ABSOLUTE)
        else()
            message(FATAL_ERROR "GLSLCC_OUTPUT_DIRECTORY not set for file: ${first_file}")
        endif()

        # use --flatten-ubos all GLSL (gles/glsl) shaders
        if (${shader_lang} STREQUAL "gles" OR ${shader_lang} STREQUAL "glsl")
            set(arg_flatten_ubo "--flatten-ubos")
        endif()

        if (${glslcc_platform_name} STREQUAL "windows")
            set(arg_bin --bin $<IF:$<CONFIG:DEBUG>,--debug,--optimize>)
        endif()

        # determine the file from the extension
        get_filename_component(file_ext ${first_file} EXT)
        get_filename_component(file_name ${first_file} NAME_WE)
        if (${source_files_count} EQUAL 1)
            if (${output_filename} STREQUAL "NOTFOUND")
                if (NOT file_type)
                    set(output_filename "${file_name}${file_ext}.${shader_lang}")
                else()
                    set(output_filename "${file_name}${file_ext}.${file_type}")
                endif()
            else()
                set(file_name "${output_filename}")
                if (NOT file_type)
                    set(output_filename "${output_filename}${file_ext}.${shader_lang}")
                else()
                    set(output_filename "${output_filename}${file_ext}.${file_type}")
                endif()  
            endif()
        else()
            if (${output_filename} STREQUAL "NOTFOUND")
                if (NOT file_type)
                    set(output_filename "${file_name}.${shader_lang}")
                else()
                    set(output_filename "${file_name}.${file_type}")
                endif()
            else()
                set(file_name "${output_filename}")
                if (NOT file_type)
                    set(output_filename "${output_filename}.${shader_lang}")
                else()
                    set(output_filename "${output_filename}.${file_type}")
                endif()  
            endif()            
        endif()

        set(output_filepath "${output_dir}/${output_filename}")
        
        # make the final args
        set(args)

        # source files
        foreach (source_file ${source_files_sub})
            get_filename_component(file_ext ${source_file} EXT)
            
            if (${file_ext} STREQUAL ".vert")
                set(args ${args} --vert=${source_file})
            elseif (${file_ext} STREQUAL ".frag")
                set(args ${args} --frag=${source_file})
            elseif (${file_ext} STREQUAL ".comp")
                set(args ${args} --compute=${source_file})
            else()
                message(FATAL_ERROR "Unknown shader file extension: ${source_file}")
            endif()
        endforeach()
        
        set(args ${args} --silent)
        set(args ${args} --lang=${shader_lang})
        if (shader_ver)
            set(args ${args} --profile=${shader_ver})
        endif()
        set(args ${args} --reflect)

        if (${file_type} STREQUAL "h")
            set(args ${args} --cvar=k_${file_name})
        elseif (${file_type} STREQUAL "sgs")
            set(args ${args} --sgs)
        endif()

        if (defs) 
            set(args ${args} --defines=${defs})
        endif()
        if (compile_flags)
            set(args ${args} ${compile_flags})
        endif()
        if (include_dirs_abs)
            set(args ${args} --include-dirs=${include_dirs_abs})
        endif()
        if (arg_flatten_ubo)
            set(args ${args} ${arg_flatten_ubo})
        endif()
        if (arg_bin)
            set(args ${args} ${arg_bin})
        endif()
        set(args ${args} --output=${output_filepath})

        string(TOUPPER ${shader_lang} shader_lang_upper)

        file(RELATIVE_PATH output_relpath ${CMAKE_CURRENT_SOURCE_DIR} ${output_filepath})

        add_custom_command(
            OUTPUT ${output_filepath}
            COMMAND ${glslcc_path}
            ARGS ${args}
            DEPENDS ${source_files_sub}
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            COMMENT "Compiling ${shader_lang_upper}: ${output_relpath}"
            VERBATIM)

        # add to source files
        target_sources(${target_name} PRIVATE "${source_files_sub}" ${output_relpath})

        if (source_group_name)
            source_group(${source_group_name} FILES ${source_files_sub})
            source_group(${source_group_name}\\${file_type} FILES ${output_relpath})
        endif()
    endforeach()    
endfunction()

function(glslcc_target_compile_shaders target_name source_files)
    glslcc__target_compile_shaders(${target_name} "" "${source_files}")
endfunction()

function(glslcc_target_compile_shaders_h target_name source_files)
    glslcc__target_compile_shaders(${target_name} "h" "${source_files}")
endfunction()

function(glslcc_target_compile_shaders_sgs target_name source_files)
    # for sgs files, we assume that all of source_files are stages of one single sgs shader
    join_array(source_files_joined "," "${source_files}")
    glslcc__target_compile_shaders(${target_name} "sgs" ${source_files_joined})
endfunction()