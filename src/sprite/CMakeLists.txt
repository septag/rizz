cmake_minimum_required(VERSION 3.1)
project(sprite)

add_library(sprite ${PLUGIN_TYPE} sprite.c 
                                  ../../include/sprite/sprite.h)
target_link_libraries(sprite PRIVATE cimgui imguizmo sx)
target_include_directories(sprite PRIVATE ../../3rdparty)

#set(files sprite.vert sprite.frag)
#set_source_files_properties(${files} PROPERTIES GLSLCC_OUTPUT_DIRECTORY "shaders_h")
#glslcc_target_compile_shaders_h(sprite "${files}")