cmake_minimum_required(VERSION 3.1)
project(astar)

set(astar_sources astar.c 
                  ../../include/rizz/astar.h
                  README.md)
rizz_add_plugin(astar "${astar_sources}")