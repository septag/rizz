add_definitions(-DEXAMPLES_ROOT="${CMAKE_CURRENT_SOURCE_DIR}")

# hello
set(HELLO_SOURCES 01-hello/hello.c)
if (BUNDLE)
    add_executable(hello ${EXEC_TYPE} ${HELLO_SOURCES})
else()
    add_library(hello SHARED ${HELLO_SOURCES})
endif()

target_link_libraries(hello PRIVATE sx)
add_dependencies(hello rizz)

# quad
set(QUAD_SOURCES 02-quad/quad.c)
if (BUNDLE)
    add_executable(quad ${EXEC_TYPE} ${QUAD_SOURCES})
else()
    add_library(quad SHARED ${QUAD_SOURCES})
endif()

target_link_libraries(quad PRIVATE sx)
add_dependencies(quad rizz)


# set any stand-alone project you want to build here (BUNDLE=1)
bundle_plugins(quad "")