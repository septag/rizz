Project(utf)
cmake_minimum_required(VERSION 3.0)

#general settings
set(UTF_SOURCES ConvertUTF.h ConvertUTF.c)

add_library(utf STATIC ${UTF_SOURCES})

