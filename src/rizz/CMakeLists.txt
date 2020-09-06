cmake_minimum_required(VERSION 3.1)
project(rizz)

set(SOURCE_FILES internal.h
                 app.c
                 core.c 
                 plugin.cpp
                 graphics.c
                 reflect.c
                 asset.c 
                 camera.c 
                 http.c 
                 vfs.c 
                 android.c
                 basisut.cpp 
                 basisut.h 
                 json.c)

set(INCLUDE_FILES ../../include/rizz/config.h
                  ../../include/rizz/sg-types.h
                  ../../include/rizz/util.h
                  ../../include/rizz/json.h 
                  ../../include/rizz/android.h
                  ../../include/rizz/ios.h 
                  ../../include/rizz/rizz.h)

set(TEXT_FILES  ../../README.md
                ../../CHANGES.md)

set(EXTERNAL_INCLUDES ../../3rdparty/sokol/sokol_app.h 
                      ../../3rdparty/sokol/sokol_gfx.h
                      ../../3rdparty/cr/cr.h
                      ../../include/cj5/cj5.h 
                      ../../include/dds-ktx/dds-ktx.h
                      ../../3rdparty/stb/stb_image.h
                      ../../3rdparty/stb/stb_image_resize.h
                      ../../3rdparty/sort/sort.h
                      ../../3rdparty/mattias/http.h 
                      ../../include/dmon/dmon.h)

if (APPLE)
    set_source_files_properties(app.c graphics.c PROPERTIES COMPILE_FLAGS "-fobjc-arc -fmodules -x objective-c") 
endif()

if (IOS)
    list(APPEND SOURCE_FILES ios.m)
endif()

# bundle: make static lib to link it to the game.exe later
# no-bundle: make rizz.exe and run the game.dll as a plugin
if (BUNDLE)
    add_library(rizz STATIC ${SOURCE_FILES} ${EXTERNAL_INCLUDES} ${INCLUDE_FILES} ${TEXT_FILES})
    if (BUNDLE_PLUGINS)
        target_link_libraries(rizz PUBLIC ${BUNDLE_PLUGINS})
    endif()    
else()
    add_executable(rizz ${SOURCE_FILES} ${EXTERNAL_INCLUDES} ${INCLUDE_FILES} ${TEXT_FILES})
    
    if (WIN32)
        set_target_properties(rizz PROPERTIES WIN32_EXECUTABLE TRUE)
    endif()
endif()

# includes and extra compile flags
target_include_directories(rizz PRIVATE ../../3rdparty)
target_link_libraries(rizz PUBLIC sx PRIVATE remotery basisu)
target_compile_definitions(rizz PRIVATE -DRIZZ_INTERNAL_API) # Expose internal API structs when build the library
if (ENABLE_HOT_LOADING)
    target_compile_definitions(rizz PRIVATE -DRIZZ_CONFIG_HOT_LOADING=1)
    if (APPLE)
        target_link_libraries(rizz PRIVATE "-framework CoreServices -framework CoreFoundation")
    endif()
else()
    target_compile_definitions(rizz PRIVATE -DRIZZ_CONFIG_HOT_LOADING=0)
endif()

# versioning
if (version)
    target_compile_definitions(rizz PRIVATE -DRIZZ_VERSION=${version})
endif()

if (RPI)
    target_include_directories(rizz PRIVATE ${CMAKE_SYSROOT}/opt/vc/include)
endif()

# Link system libs
if (ANDROID)
    target_link_libraries(rizz PRIVATE EGL android GLESv3)
elseif (RPI)
    target_link_directories(rizz PUBLIC ${CMAKE_SYSROOT}/opt/vc/lib)
    target_link_libraries(rizz PRIVATE GLESv2 EGL bcm_host)
elseif (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    target_link_libraries(rizz PRIVATE flextGL X11 GL)
elseif (WIN32)
    target_link_libraries(rizz PRIVATE dxgi d3d11)
endif()
