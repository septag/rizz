## astar plugin

Provides grid based astar pathfinding. For demo, see **09-pathfind** example.

### Usage

1. prepare rizz_astar_world struct and allocate cell data :

```c
uint8_t celldata[16 * 16] = {
    1,1,1,1,1,1,1,1,1,0,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,0,0,0,0,1,1,1,
    1,1,0,0,0,0,0,0,0,0,0,0,0,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,0,0,1,0,0,1,1,1,1,1,1,
    1,1,1,1,1,0,0,1,0,0,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,0,0,1,0,0,1,1,1,1,1,1,
    1,1,1,1,1,0,0,1,0,0,1,1,1,1,1,1,
    1,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,0,1,1,1,1,1,1,0,0,0,0,0,1,1,1,
    1,0,1,1,1,1,1,1,0,0,0,0,0,1,1,1,
    1,0,1,1,0,0,0,0,0,0,0,0,0,1,1,1,
    1,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
};

rizz_astar_world my_world = (rizz_astar_world){
    .cells = celldata,
    .width = 16,
    .height = 16,
    .scale = 1,
    .offset = SX_VEC2_ZERO,
};
```

2. prepare rizz_astar_agent struct and set cell costs on it :

```c
enum {
    CELLTYPE_BLOCKED = 0,
    CELLTYPE_GRASS,
    CELLTYPE_ROAD,
    CELLTYPE_WATER,
};

rizz_astar_agent my_agent = (rizz_astar_agent){
    .costs[CELLTYPE_BLOCKED] = 0, 
    .costs[CELLTYPE_GRASS] = 10,
    .costs[CELLTYPE_ROAD] = 1,
    .costs[CELLTYPE_WATER] = 0, // cost 0 means this cell is non walkable
};
```

3. now you can use the_astar->findpath function :

```c
sx_vec2 start = sx_vec2f(0, 0);
sx_vec2 end = sx_vec2f(16, 16);

rizz_astar_path my_path = (rizz_astar_path){
    .alloc = the_core->alloc(RIZZ_MEMID_GAME),
    .array = NULL, // the_astar->findpath will allocate and fill a sx_array
};

if (the_astar->findpath(&my_world, &my_agent, start, end, &my_path)) {
    // do something with the path ...
    sx_array_free(my_path.alloc, my_path.array); // free path
} else { 
    // path not found
}
```
