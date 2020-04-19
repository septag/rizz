Project(basisu)
cmake_minimum_required(VERSION 3.0)

add_library(basisu STATIC basisu_transcoder.h
                          basisu_file_headers.h
                          basisu_global_selector_cb.h
                          basisu_transcoder_internal.h 
                          basisu.h
                          basisu_transcoder.cpp)

if (MSVC)
    target_compile_options(basisu PRIVATE /wd4190)
    if (CLANG_CL)
        target_compile_options(basisu PRIVATE -Wno-unused-variable)
    endif()
else()
    target_compile_options(basisu PRIVATE -fno-strict-aliasing)
endif()

target_compile_definitions(basisu PRIVATE 
    -DBASISD_SUPPORT_ASTC=0 
    -DBASISD_SUPPORT_ATC=0 
    -DBASISD_SUPPORT_FXT1=0        
    -DBASISD_SUPPORT_BC7=0 
    -DBASISD_SUPPORT_BC7_MODE6_OPAQUE_ONLY=0 
    -DBASISD_SUPPORT_PVRTC2=0
    -DBASISD_SUPPORT_ASTC_HIGHER_OPAQUE_QUALITY=0)

# ignore formats that are not related to the platform
if (ANDROID OR IOS)
    target_compile_definitions(basisu PRIVATE 
        -DBASISD_SUPPORT_DXT1=0 
        -DBASISD_SUPPORT_DXT5A=0 
        -DBASISD_SUPPORT_BC7_MODE5=0)
    if (ANDROID)
        target_compile_definitions(basisu PRIVATE -DBASISD_SUPPORT_PVRTC1=0)
    endif()
else()
    target_compile_definitions(basisu PRIVATE -DBASISD_SUPPORT_ETC2_EAC_A8=0)
endif()