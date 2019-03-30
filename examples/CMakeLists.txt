# hello\
set(HELLO_SOURCES 01-hello/hello.c)
if (BUNDLE)
    add_executable(hello ${EXEC_TYPE} ${HELLO_SOURCES})
else()
    add_library(hello SHARED ${HELLO_SOURCES})
endif()

target_link_libraries(hello PRIVATE sx)
add_dependencies(hello rizz)

# set any stand-alone project you want to build here (BUNDLE=1)
bundle_plugins(hello "")