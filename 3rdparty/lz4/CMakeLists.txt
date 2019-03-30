Project(lz4)
cmake_minimum_required(VERSION 3.0)

#general settings
set(LZ4_SOURCES lz4.h lz4.c lz4hc.c lz4hc.h)

add_library(lz4 STATIC ${LZ4_SOURCES})

