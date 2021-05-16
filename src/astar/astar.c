#include "rizz/astar.h"

#include "sx/array.h"
#include "sx/bheap.h"
#include "sx/string.h"
#include "sx/math-vec.h"

#define ASTAR_DEFAULT_COST 10
#define ASTAR_DIAGONAL_COST 14
#define ASTAR_CHANGE_DIR_COST 10

RIZZ_STATE static uint32_t g_maxsearch = 10000;
static int k_dirs[8][2] = { { 0, -1 }, { 1, 0 }, { 0, 1 },  { -1, 0 },
                            { 1, -1 }, { 1, 1 }, { -1, 1 }, { -1, -1 } };

RIZZ_STATE static rizz_api_core* the_core;
RIZZ_STATE static rizz_api_plugin* the_plugin;

typedef union {
    int32_t id;
    struct {
        uint16_t x;
        uint16_t y;
    };
} loc;

typedef struct {
    int32_t g, f;
    loc p;
    uint8_t stat;
} cell;

static inline void gridcoord(const rizz_astar_world* world, sx_vec2 wpos, loc* _loc)
{
    wpos = sx_vec2_sub(wpos, world->offset);
    wpos = sx_vec2_mulf(wpos, 1.0f / world->scale);
    wpos.x = sx_round(wpos.x);
    wpos.y = sx_round(wpos.y);
    int xmax = world->width - 1;
    int ymax = world->height - 1;
    int x = (int)wpos.x;
    int y = (int)wpos.y;
    *_loc = (loc){
        .x = (uint16_t)sx_clamp(x, 0, xmax),
        .y = (uint16_t)sx_clamp(y, 0, ymax),
    };
}

static inline sx_vec2 worldcoord(const rizz_astar_world* world, loc _loc)
{
    float xx = (_loc.x * world->scale) + world->offset.x;
    float yy = (_loc.y * world->scale) + world->offset.y;
    return sx_vec2f(xx, yy);
}

static inline int abs__(int v) { return v >= 0 ? v : -v; }

static inline int heuristic(loc a, loc b)
{
    int dx = abs__(a.x - b.x);
    int dy = abs__(a.y - b.y);
    int ortho = abs__(dx - dy);
    int diag = abs__(((dx + dy) - ortho) / 2);
    return diag + ortho + dx + dy;
}

static void astar__set_maxsearch(uint32_t value) { g_maxsearch = value; }

static bool astar__findpath(const rizz_astar_world* world, const rizz_astar_agent* agent,
                            sx_vec2 start, sx_vec2 end, rizz_astar_path* path)
{
#define GRID_ITEM(_loc) (&calcgrid[(_loc).x + (_loc).y * world->width])
    const uint8_t openv = 1, closev = 2;
    const sx_alloc* talloc = the_core->tmp_alloc_push();
    int ret = -1;   // other than -1 exits the main loop, =0 means success

    sx_scope(the_core->tmp_alloc_pop()) {
        loc sloc, eloc;
        gridcoord(world, start, &sloc);
        gridcoord(world, end, &eloc);

        sx_bheap* openlist = sx_bheap_create(talloc, world->width * world->height);
        cell* calcgrid = sx_malloc(talloc, sizeof(cell) * world->width * world->height);
        sx_memset(calcgrid, 0, sizeof(cell) * world->width * world->height);

        cell* scell = GRID_ITEM(sloc);
        *scell = (cell){ .g = 0, .f = heuristic(sloc, eloc), .p = sloc };
        sx_bheap_push_min(openlist, scell->f, scell);

        uint32_t num_search = 0;
        bool found = false;
        while (!sx_bheap_empty(openlist) && ret == -1) {
            cell* ccell = (cell*)sx_bheap_pop_min(openlist).user;
            loc cloc = (loc){
                .x = (uint16_t)((uint64_t)(ccell - calcgrid) % world->width),
                .y = (uint16_t)((uint64_t)(ccell - calcgrid) / world->width),
            };
            ccell->stat = closev;

            if (cloc.id == eloc.id) {
                found = true;
                break;
            }

            loc cdir = (loc){
                .x = (uint16_t)(ccell->p.x - cloc.x),
                .y = (uint16_t)(ccell->p.y - cloc.y),
            };

            for (int32_t i = 0; i < 8; i++) {
                loc nloc = (loc){
                    .x = (uint16_t)(cloc.x + k_dirs[i][0]),
                    .y = (uint16_t)(cloc.y + k_dirs[i][1]),
                };

                if (nloc.x >= world->width || nloc.y >= world->height)
                    continue;

                uint8_t cost = agent->costs[world->cells[nloc.x + nloc.y * world->width]];
                if (cost == 0)
                    continue;

                cell* ncell = GRID_ITEM(nloc);
                if (ncell->stat == closev)
                    continue;

                int32_t ng = ccell->g + cost * (i > 3 ? ASTAR_DIAGONAL_COST : ASTAR_DEFAULT_COST);

                loc ndir = (loc){
                    .x = (uint16_t)(cloc.x - nloc.x),
                    .y = (uint16_t)(cloc.y - nloc.y),
                };

                if (cdir.id != ndir.id)
                    ng += ASTAR_CHANGE_DIR_COST;

                if (ncell->stat == 0 || ncell->g > ng) {
                    ncell->g = ng;
                    ncell->f = ng + heuristic(nloc, eloc);
                    ncell->p = cloc;
                    if (ncell->stat == 0)
                        sx_bheap_push_min(openlist, ncell->f, ncell);

                    ncell->stat = openv;
                }

            }    // for
            if (num_search++ > g_maxsearch) {
                ret = 1;
                break;
            }
        }    // while

        if (found) {
            sx_array_clear(path->array);
            loc cloc = eloc;
            cell* ccell = GRID_ITEM(cloc);
            int dx = 0, dy = 0;
            while (cloc.id != ccell->p.id) {
                int dx2 = (int)cloc.x - (int)ccell->p.x;
                int dy2 = (int)cloc.y - (int)ccell->p.y;
                if (dx2 != dx || dy2 != dy) {
                    sx_array_push(path->alloc, path->array, worldcoord(world, cloc));
                    dx = dx2;
                    dy = dy2;
                }
                cloc = ccell->p;
                ccell = GRID_ITEM(cloc);
            }
            if (((int)cloc.x - (int)ccell->p.x) != dx || ((int)cloc.y - (int)ccell->p.y) != dy) {
                sx_array_push(path->alloc, path->array, worldcoord(world, sloc));
            }

            { // reverse array
                int count = sx_array_count(path->array);
                int half = count / 2;
                for (int i = 0; i < half; i++) {
                    int ri = count - i - 1;
                    sx_vec2 tmp = path->array[i];
                    path->array[i] = path->array[ri];
                    path->array[ri] = tmp;
                }
            }
            
            ret = 0;
        } else {
            ret = 1;
        }
    } // scope

    return ret == 0;
}

static rizz_api_astar the__astar = {
    .set_maxsearch = astar__set_maxsearch,
    .findpath = astar__findpath,
};

rizz_plugin_decl_main(astar, plugin, e)
{
    switch (e) {
    case RIZZ_PLUGIN_EVENT_STEP:
        break;
    case RIZZ_PLUGIN_EVENT_INIT:
        the_plugin = plugin->api;
        the_core = the_plugin->get_api(RIZZ_API_CORE, 0);

        the_plugin->inject_api("astar", 0, &the__astar);
        break;
    case RIZZ_PLUGIN_EVENT_LOAD:
        the_plugin->inject_api("astar", 0, &the__astar);
        break;
    case RIZZ_PLUGIN_EVENT_UNLOAD:
        break;
    case RIZZ_PLUGIN_EVENT_SHUTDOWN:
        the_plugin->remove_api("astar", 0);
        break;
    }

    return 0;
}

rizz_plugin_implement_info(astar, 1000, "grid based astar pathfinding", NULL, 0);