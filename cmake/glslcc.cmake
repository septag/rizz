string(TOLOWER ${CMAKE_SYSTEM_NAME} glslcc_platform_name)
if (IOS)
    set(glslcc_platform_name "ios")
endif()

# resolve glslcc binary
if (NOT GLSLCC_BIN AND NOT glslcc_path)
    # try to find glslcc executable path
    # also take a guess (most likely for personal projects)
    if (WIN32)
        set(tools_dir "${PROJECT_SOURCE_DIR}/tools/windows")    
    elseif(APPLE)
        set(tools_dir "${PROJECT_SOURCE_DIR}/tools/darwin")
    elseif(UNIX)
        set(tools_dir "${PROJECT_SOURCE_DIR}/tools/linux")
    endif()

    find_program(glslcc_path 
                 NAMES glslcc
                 PATHS ${tools_dir} ENV PATH
                 DOC "path to glslcc binary")

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
                FULL_DOCS "Compiled shader output filename. Default is SOURCE_FILE.EXT.h")
# PROPERTY: extra flags (optional)
define_property(SOURCE PROPERTY GLSLCC_COMPILE_FLAGS 
                BRIEF_DOCS "Extra compile flags"
                FULL_DOCS "Extra compile flags")

function(join_array output glue values)
    string (REGEX REPLACE "([^\\]|^);" "\\1${glue}" tmp_str "${values}")
    string (REGEX REPLACE "[\\](.)" "\\1" tmp_str "${tmp_str}") #fixes escaping
    set (${output} "${tmp_str}" PARENT_SCOPE)
endfunction()        

function(glslcc__target_compile_shaders target_name file_type source_files)
    foreach(source_file ${source_files})
        get_source_file_property(defs ${source_file} COMPILE_DEFINITIONS) 
        get_source_file_property(include_dirs ${source_file} INCLUDE_DIRECTORIES) 
        get_source_file_property(output_dir ${source_file} GLSLCC_OUTPUT_DIRECTORY)
        get_source_file_property(shader_lang ${source_file} GLSLCC_SHADER_LANG)
        get_source_file_property(shader_ver ${source_file} GLSLCC_SHADER_VERSION)
        get_source_file_property(output_filename ${source_file} GLSLCC_OUTPUT_FILENAME)
        get_source_file_property(compile_flags ${source_file} GLSLCC_COMPILE_FLAGS)

        if (include_dirs)
            set(include_dirs_abs)
            foreach(include_dir ${include_dirs})
                if (IS_DIRECTORY ${include_dir})
                    get_filename_component(include_dir ${include_dir} ABSOLUTE)
                    list(APPEND include_dirs_abs ${include_dir})
                endif()
            endforeach()
            join_array(include_dirs_abs $<SEMICOLON> "${include_dirs_abs}")
        endif()

        if (defs)
            join_array(defs ";" "${defs}")
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
            elseif (${glslcc_platform_name} STREQUAL "linux")
                set(shader_ver "330")
            endif()
        endif()

        if (output_dir)
            # append system_name to output_path
            set(output_dir ${output_dir}/${shader_lang})

            if (NOT IS_DIRECTORY ${output_dir})
                file(MAKE_DIRECTORY ${output_dir})
            endif()
            get_filename_component(output_dir ${output_dir} ABSOLUTE)
        else()
            message(FATAL_ERROR "GLSLCC_OUTPUT_DIRECTORY not set for file: ${source_file}")
        endif()

        # use --flatten-ubos all GLSL (gles/glsl) shaders
        if (${shader_lang} STREQUAL "gles" OR ${shader_lang} STREQUAL "glsl")
            set(arg_flatten_ubo "--flatten-ubos")
        endif()

        if (${glslcc_platform_name} STREQUAL "windows")
            set(arg_bin "--bin")
        endif()

        # determine the file from the extension
        get_filename_component(file_ext ${source_file} EXT)
        get_filename_component(file_name ${source_file} NAME_WE)
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

        set(output_filepath "${output_dir}/${output_filename}")
        

        # make the final args
        set(args)
        if (${file_ext} STREQUAL ".vert")
            set(args --vert=${source_file})
        elseif (${file_ext} STREQUAL ".frag")
            set(args --frag=${source_file})
        elseif (${file_ext} STREQUAL ".comp")
            set(args --comp=${source_file})
        else()
            message(FATAL_ERROR "Unknown shader file extension: ${source_file}")
        endif()
        
        set(args ${args} --silent)
        set(args ${args} --lang=${shader_lang})
        set(args ${args} --profile=${shader_ver})
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
            DEPENDS ${source_file}
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            COMMENT "Compiling ${shader_lang_upper}: ${output_relpath}"
            VERBATIM)

        # add to source files
        target_sources(${target_name} PRIVATE ${source_file} ${output_relpath})
    endforeach()    
endfunction()

function(glslcc_target_compile_shaders target_name source_files)
   glslcc__target_compile_shaders(${target_name} "" "${source_files}")
endfunction()

function(glslcc_target_compile_shaders_h target_name source_files)
   glslcc__target_compile_shaders(${target_name} "h" "${source_files}")
endfunction()

function(glslcc_target_compile_shaders_sgs target_name source_files)
   glslcc__target_compile_shaders(${target_name} "sgs" "${source_files}")
endfunction()