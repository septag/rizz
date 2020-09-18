#pragma once

#include "rizz/rizz.h"

#ifndef RIZZ_ASTAR_MAX_CELLTYPE
#    define RIZZ_ASTAR_MAX_CELLTYPE 16
#endif

typedef struct rizz_astar_agent {
    uint8_t costs[RIZZ_ASTAR_MAX_CELLTYPE];
} rizz_astar_agent;

typedef struct rizz_astar_world {
    uint8_t* cells;
    sx_vec2 offset;
    float scale;
    uint16_t width, height;
} rizz_astar_world;

typedef struct rizz_astar_path {
    const sx_alloc* alloc;
    /* sx_array */ sx_vec2* array;
} rizz_astar_path;

typedef struct rizz_api_astar {
    void (*set_maxsearch)(uint32_t value);
    bool (*findpath)(const rizz_astar_world* world, const rizz_astar_agent* agent, sx_vec2 start,
                  sx_vec2 end, rizz_astar_path* path);
} rizz_api_astar;