cmake_minimum_required(VERSION 3.0)
project(remotery)

set(SOURCE_FILES lib/Remotery.c lib/Remotery.h)

if (APPLE)
    list(APPEND SOURCE_FILES lib/RemoteryMetal.mm)
endif()

add_library(remotery STATIC ${SOURCE_FILES})
target_include_directories(remotery PUBLIC lib)

if (APPLE)
    target_compile_definitions(remotery PUBLIC -DRMT_USE_METAL=1)
elseif (ANDROID)
    target_compile_definitions(remotery PUBLIC -DRMT_USE_OPENGL=0)
elseif (RPI)
    target_compile_definitions(remotery PUBLIC -DRMT_USE_OPENGL=0)
elseif (WIN32)
    target_compile_definitions(remotery PUBLIC -DRMT_USE_D3D11=1)
elseif ((${CMAKE_SYSTEM_NAME} MATCHES "Linux"))
    target_compile_definitions(remotery PUBLIC -DRMT_USE_OPENGL=1)
endif()

if (NOT ENABLE_PROFILER)
    target_compile_definitions(remotery PUBLIC -DRMT_ENABLED=0)
endif()

if (NOT MSVC OR (MSVC AND CMAKE_C_COMPILER_ID MATCHES "Clang"))
    target_compile_options(remotery PRIVATE -Wno-switch -Wno-uninitialized -Wno-unused-function)
elseif (MSVC)
    target_compile_options(remotery PRIVATE /wd5105)
endif()
