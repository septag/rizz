cmake_minimum_required(VERSION 3.1)
project(collision)

set(collision_sources collision.c 
                      ../../include/rizz/collision.h
                      README.md)
rizz_add_plugin(collision "${collision_sources}")
