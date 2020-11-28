cmake_minimum_required(VERSION 3.1)
project(sound)

set(sound_sources sound.c 
                  stb_vorbis.c 
                  ../../include/rizz/sound.h 
                  ../../3rdparty/sokol/sokol_audio.h 
                  ../../3rdparty/dr_libs/dr_wav.h 
                  README.md)
rizz_add_plugin(sound "${sound_sources}")

if (CMAKE_C_COMPILER_ID MATCHES "GNU")
    set_source_files_properties(stb_vorbis.c PROPERTIES COMPILE_FLAGS "-Wno-shadow -Wno-maybe-uninitialized -Wno-type-limits")
elseif (CMAKE_C_COMPILER_ID MATCHES "Clang")
    set_source_files_properties(stb_vorbis.c PROPERTIES COMPILE_FLAGS "-Wno-shadow -Wno-uninitialized -Wno-unused-parameter")
elseif (MSVC)
    set_source_files_properties(stb_vorbis.c PROPERTIES COMPILE_FLAGS /W3)
endif()

if (APPLE) 
    target_link_libraries(sound PRIVATE "-framework CoreAudio")
    set_source_files_properties(sound.c PROPERTIES COMPILE_FLAGS "-fobjc-arc -fmodules -x objective-c")
elseif (ANDROID)
    target_link_libraries(sound PRIVATE -lOpenSLES)
elseif (${CMAKE_SYSTEM_NAME} MATCHES "Linux")    
    target_link_libraries(sound PRIVATE -lasound)
endif()

target_compile_definitions(sound PRIVATE -DSTB_VORBIS_NO_PUSHDATA_API)
