get_directory_property(has_parent PARENT_DIRECTORY)
if (has_parent) 
    set (RIZZ_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/rizz/ PARENT_SCOPE)
else()
    set (RIZZ_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/rizz/)
endif()

function(generate_bundle_file_internal plugins filepath func_name game_name)
    message(STATUS "generating header file: ${filepath}")
    set (code "// DO NOT MODIFY: This file is auto-generated by bundle.cmake\n")
    set (code "${code}#pragma once\n")
    set (code "${code}#include \"rizz/rizz.h\"\n\n")

    if (game_name)
        set (code "${code}#define ENTRY_NAME \"${game_name}\"\n\n")
    endif()

    set (code "${code}#if defined(PLUGIN_SOURCE)\n")
    # fwd declare
    set (code "${code}static void rizz__plugin_register(const rizz_plugin_info*)\;\n")
    foreach (plugin ${plugins})
        string(REPLACE "-" "_" plugin_fixed ${plugin})
        set (code "${code}RIZZ_PLUGIN_EXPORT int rizz_plugin_main_${plugin_fixed}(rizz_plugin*, rizz_plugin_event)\;\n")
        set (code "${code}RIZZ_PLUGIN_EXPORT void rizz_plugin_event_handler_${plugin_fixed}(const rizz_app_event*)\;\n")
        if (NOT "${plugin}" STREQUAL "${game_name}")
            set (code "${code}RIZZ_PLUGIN_EXPORT void rizz_plugin_get_info_${plugin_fixed}(rizz_plugin_info*)\;\n")
        endif()
    endforeach()
    set (code "${code}\n")

    # start 
    set (code "${code}static inline void ${func_name}() {\n")
    set (code "${code}\trizz_plugin_info info\;\n\n")

    foreach (plugin ${plugins})
        string(REPLACE "-" "_" plugin_fixed ${plugin})
        if (NOT "${plugin}" STREQUAL "${game_name}")
            set (code "${code}\trizz_plugin_get_info_${plugin_fixed}(&info)\;\n")
        else()
            set (code "${code}\tsx_memset(&info, 0x0, sizeof(info))\;\n")
            set (code "${code}\tsx_strcpy(info.name, sizeof(info.name), \"${plugin}\")\;\n")
        endif()
        set (code "${code}\tinfo.main_cb = rizz_plugin_main_${plugin_fixed}\;\n")
        set (code "${code}\tinfo.event_cb = rizz_plugin_event_handler_${plugin_fixed}\;\n")
        set (code "${code}\trizz__plugin_register(&info)\;\n")
    endforeach()

    set(code "${code}}\n")
    set(code "${code}#endif\n")
    file (WRITE ${filepath} ${code})
endfunction()

# arguments:
# NAME actual internal name
# PLUGINS plugins
function(rizz_set_executable target_name)
    # parse aguments
    set(plugins)
    set(internal_name ${target_name})
    set(mode)
    foreach (arg ${ARGN}) 
        if ("${arg}" STREQUAL "PLUGINS")
            set(mode "plugins")
        elseif ("${arg}" STREQUAL "NAME")
            set(mode "name")
        else()
            if ("${mode}" STREQUAL "plugins")
                list(APPEND plugins ${arg})
            elseif ("${mode}" STREQUAL "name")
                set(internal_name ${arg})
            endif()
        endif()
    endforeach()

    if (BUNDLE)
        if (plugins)
            # target_link_libraries(${target_name} PRIVATE ${plugins})
            list(APPEND plugins ${internal_name})
        else()
            set(plugins ${internal_name})
        endif()
        
        generate_bundle_file_internal("${plugins}" ${RIZZ_SOURCE_DIR}plugin_bundle.h 
                                      "rizz__plugin_bundle" ${internal_name})
        target_sources(rizz PRIVATE ${RIZZ_SOURCE_DIR}plugin_bundle.h)
        target_link_libraries(${target_name} PRIVATE rizz)   # bundle mode always links with the rizz
        target_compile_definitions(${target_name} PRIVATE -DRIZZ_BUNDLE)
    elseif (plugins)
        add_dependencies(${target_name} ${plugins})
    endif()    
endfunction()