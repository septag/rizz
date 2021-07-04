#include "rizz/config.h"
#include "rizz/utility.h"
#include "sx/allocator.h"
#include "sx/io.h"
#include "sx/linear-buffer.h"
#include "sx/math-vec.h"
#include "sx/os.h"
#include "sx/rng.h"
#include "sx/string.h"

#include "rizz/3dtools.h"
#include "rizz/imgui-extra.h"
#include "rizz/imgui.h"
#include "rizz/rizz.h"

#include <alloca.h>

#include "../common.h"

RIZZ_STATE static rizz_api_core* the_core;
RIZZ_STATE static rizz_api_gfx* the_gfx;
RIZZ_STATE static rizz_api_app* the_app;
RIZZ_STATE static rizz_api_imgui* the_imgui;
RIZZ_STATE static rizz_api_asset* the_asset;
RIZZ_STATE static rizz_api_camera* the_camera;
RIZZ_STATE static rizz_api_vfs* the_vfs;
RIZZ_STATE static rizz_api_3d* the_3d;
RIZZ_STATE static rizz_api_imgui_extra* the_imguix;
RIZZ_STATE static rizz_api_utility* the_utility;

#ifndef EXAMPLES_ROOT
#    define EXAMPLES_ROOT ""
#endif

#define NUM_BOIDS 2048
#define MAX_NEIGHBORS 64

#define INIT_SPEED 2.0f
#define MIN_SPEED 2.0f
#define MAX_SPEED 5.0f
#define NEIGHBOR_DIST 1.0f
#define NEIGHBOR_FOV 90.0f

#define SEPERATION_WEIGHT 8.0f
#define WALL_SIZE 5.0f
#define WALL_DIST 2.0f
#define WALL_WEIGHT 1.0f
#define OBSTACLE_WEIGHT 16.0f
#define ALIGNMENT_WEIGHT 2.0f
#define COHESION_WEIGHT 3.0f

typedef struct {
    int count;
    int indices[MAX_NEIGHBORS];
} neighbors_t;

bool add_neighbor(neighbors_t* neighbors, int ni)
{
    if (neighbors->count == MAX_NEIGHBORS)
        return false;

    neighbors->indices[neighbors->count++] = ni;
    return true;
}

void clear_neighbors(neighbors_t* neighbors)
{
    neighbors->count = 0;
}

typedef struct {
    int count;

    neighbors_t* neighbors;
    sx_vec3* pos;
    sx_vec3* vel;
    sx_vec3* acl;
    sx_color* col;
} boid_t;

typedef struct {
    rizz_gfx_stage stage;
    rizz_camera_fps cam;
    sx_vec3 obstacle_origin;
    float obstacle_radius;
    boid_t* boids;
    bool use_jobs;
} boid_simulation_t;

RIZZ_STATE static boid_simulation_t g_simulation;

static inline sx_vec3 calc_wall_acl(float dist, sx_vec3 dir)
{
    if (dist < WALL_DIST) {
        float f = dist / WALL_DIST;
        return sx_vec3_mulf(dir, WALL_WEIGHT / sx_abs(f));
    }
    return SX_VEC3_ZERO;
}

void update_boid(int bi, float dt)
{
    // neighbors
    {
        sx_vec3 bpos = g_simulation.boids->pos[bi];
        for (int i = 0; i < g_simulation.boids->count; i++) {
            if (i == bi)
                continue;

            sx_vec3 npos = g_simulation.boids->pos[i];
            sx_vec3 nvel = g_simulation.boids->vel[i];
            sx_vec3 to = sx_vec3_sub(bpos, npos);
            float len = sx_vec3_len(to);
            if (len < NEIGHBOR_DIST) {
                sx_vec3 fwd = sx_vec3_norm(nvel);
                sx_vec3 dir = sx_vec3_norm(to);
                if (sx_vec3_dot(fwd, dir) > 0) {
                    if (!add_neighbor(&g_simulation.boids->neighbors[bi], i))
                        break;
                }
            }
        }
    }

    //  walls
    {
        float scale = WALL_SIZE;
        sx_vec3 bpos = g_simulation.boids->pos[bi];

        sx_vec3 xp = calc_wall_acl(-scale - bpos.x, sx_vec3f(1, 0, 0));
        sx_vec3 yp = calc_wall_acl(-scale - bpos.y, sx_vec3f(0, 1, 0));
        sx_vec3 zp = calc_wall_acl(-scale - bpos.z, sx_vec3f(0, 0, 1));
        sx_vec3 xn = calc_wall_acl(+scale - bpos.x, sx_vec3f(-1, 0, 0));
        sx_vec3 yn = calc_wall_acl(+scale - bpos.y, sx_vec3f(0, -1, 0));
        sx_vec3 zn = calc_wall_acl(+scale - bpos.z, sx_vec3f(0, 0, -1));

        sx_vec3 r = SX_VEC3_ZERO;
        r = sx_vec3_add(r, xp);
        r = sx_vec3_add(r, yp);
        r = sx_vec3_add(r, zp);
        r = sx_vec3_add(r, xn);
        r = sx_vec3_add(r, yn);
        r = sx_vec3_add(r, zn);

        g_simulation.boids->acl[bi] = sx_vec3_add(g_simulation.boids->acl[bi], r);
    }

    // avoid obstacle
    {
        sx_vec3 bpos = g_simulation.boids->pos[bi];
        float dist;
        sx_vec3 dir = sx_vec3_norm_len(sx_vec3_sub(bpos, g_simulation.obstacle_origin), &dist);
        if (dist < g_simulation.obstacle_radius)
        {
            g_simulation.boids->acl[bi] = sx_vec3_add(g_simulation.boids->acl[bi], sx_vec3_mulf(dir, OBSTACLE_WEIGHT));
        }
    }

    // seperation
    {
        neighbors_t* neighbors = &g_simulation.boids->neighbors[bi];

        if (neighbors->count == 0)
            return;

        sx_vec3 force = SX_VEC3_ZERO;
        sx_vec3 bpos = g_simulation.boids->pos[bi];
        for (int i = 0; i < neighbors->count; i++) {
            sx_vec3 npos = g_simulation.boids->pos[neighbors->indices[i]];
            force = sx_vec3_add(force, sx_vec3_norm(sx_vec3_sub(bpos, npos)));
        }
        force.x /= (float)neighbors->count;
        force.y /= (float)neighbors->count;
        force.z /= (float)neighbors->count;

        force = sx_vec3_mulf(force, SEPERATION_WEIGHT);
        g_simulation.boids->acl[bi] = sx_vec3_add(g_simulation.boids->acl[bi], force);
    }

    // alignment
    {
        neighbors_t* neighbors = &g_simulation.boids->neighbors[bi];

        if (neighbors->count == 0)
            return;

        sx_vec3 average = SX_VEC3_ZERO;
        for (int i = 0; i < neighbors->count; i++) {
            sx_vec3 nvel = g_simulation.boids->vel[neighbors->indices[i]];
            average = sx_vec3_add(average, nvel);
        }
        average.x /= (float)neighbors->count;
        average.y /= (float)neighbors->count;
        average.z /= (float)neighbors->count;

        sx_vec3 r = sx_vec3_sub(average, g_simulation.boids->vel[bi]);
        r = sx_vec3_mulf(r, ALIGNMENT_WEIGHT);
        g_simulation.boids->acl[bi] = sx_vec3_add(g_simulation.boids->acl[bi], r);
    }

    // cohesion
    {
        neighbors_t* neighbors = &g_simulation.boids->neighbors[bi];

        if (neighbors->count == 0)
            return;

        sx_vec3 average = SX_VEC3_ZERO;
        for (int i = 0; i < neighbors->count; i++) {
            sx_vec3 npos = g_simulation.boids->pos[neighbors->indices[i]];
            average = sx_vec3_add(average, npos);
        }
        average.x /= (float)neighbors->count;
        average.y /= (float)neighbors->count;
        average.z /= (float)neighbors->count;

        sx_vec3 r = sx_vec3_sub(average, g_simulation.boids->pos[bi]);
        r = sx_vec3_mulf(r, COHESION_WEIGHT);
        g_simulation.boids->acl[bi] = sx_vec3_add(g_simulation.boids->acl[bi], r);
    }

    // apply move
    {
        sx_vec3 pos = g_simulation.boids->pos[bi];
        sx_vec3 vel = g_simulation.boids->vel[bi];
        sx_vec3 acl = g_simulation.boids->acl[bi];
        vel = sx_vec3_add(vel, sx_vec3_mulf(acl, dt));
        float speed = sx_vec3_len(vel);
        sx_vec3 dir = sx_vec3_norm(vel);
        vel = sx_vec3_mulf(dir, sx_clamp(speed, MIN_SPEED, MAX_SPEED));
        pos = sx_vec3_add(pos, sx_vec3_mulf(vel, dt));
        acl = SX_VEC3_ZERO;

        g_simulation.boids->pos[bi] = pos;
        g_simulation.boids->vel[bi] = vel;
        g_simulation.boids->acl[bi] = acl;
    }
}

static bool init()
{
#if SX_PLATFORM_ANDROID
    the_vfs->mount_android_assets("/assets");
#else
    // mount `/asset` directory
    char asset_dir[RIZZ_MAX_PATH];
    sx_os_path_join(asset_dir, sizeof(asset_dir), EXAMPLES_ROOT, "assets");    // "/examples/assets"
    the_vfs->mount(asset_dir, "/assets");
#endif

    // register main graphics stage.
    // at least one stage should be registered if you want to draw anything
    g_simulation.stage = the_gfx->stage_register("main", (rizz_gfx_stage){ .id = 0 });
    sx_assert(g_simulation.stage.id);

    // camera
    // projection: setup for perspective
    // view: Z-UP Y-Forward (like blender)
    sx_vec2 screen_size;
    the_app->window_size(&screen_size);
    const float view_width = screen_size.x;
    const float view_height = screen_size.y;
    the_camera->fps_init(&g_simulation.cam, 60.0f, sx_rectwh(0, 0, view_width, view_height), 0.1f, 500.0f);
    the_camera->fps_lookat(&g_simulation.cam, sx_vec3f(5, 15, 5), SX_VEC3_ZERO, SX_VEC3_UNITZ);

    // init boids
    {
        const int boid_count = NUM_BOIDS;
        sx_linear_buffer lb;
        sx_linear_buffer_init(&lb, boid_t, 0);
        sx_linear_buffer_addtype(&lb, boid_t, neighbors_t, neighbors, boid_count, 0);
        sx_linear_buffer_addtype(&lb, boid_t, sx_vec3, pos, boid_count, 0);
        sx_linear_buffer_addtype(&lb, boid_t, sx_vec3, vel, boid_count, 0);
        sx_linear_buffer_addtype(&lb, boid_t, sx_vec3, acl, boid_count, 0);
        sx_linear_buffer_addtype(&lb, boid_t, sx_color, col, boid_count, 0);

        boid_t* buf = sx_linear_buffer_calloc(&lb, the_core->alloc(RIZZ_MEMID_GAME));
        buf->count = boid_count;
        g_simulation.boids = buf;
        sx_rng rng;
        sx_rng_seed(&rng, 123);
        for (int i = 0; i < boid_count; i++) {
            sx_vec3 pos = sx_vec3f(sx_rng_genf(&rng), sx_rng_genf(&rng), sx_rng_genf(&rng));
            pos.x = (pos.x * 2 - 1);
            pos.y = (pos.y * 2 - 1);
            pos.z = (pos.z * 2 - 1);
            pos = sx_vec3_mulf(sx_vec3_norm(pos), sx_rng_genf(&rng));

            sx_color col =
                sx_color4f(sx_rng_genf(&rng), sx_rng_genf(&rng), sx_rng_genf(&rng), 1.0f);

            g_simulation.boids->pos[i] = pos;
            g_simulation.boids->col[i] = col;

            sx_vec3 vel = sx_vec3f(sx_rng_genf(&rng), sx_rng_genf(&rng), sx_rng_genf(&rng));
            vel.x = (vel.x * 2 - 1);
            vel.y = (vel.y * 2 - 1);
            vel.z = (vel.z * 2 - 1);
            vel = sx_vec3_norm(vel);
            g_simulation.boids->vel[i] = sx_vec3_mulf(vel, INIT_SPEED);
        }
    }

    the_3d->debug.set_max_instances(NUM_BOIDS + 16);
    g_simulation.obstacle_radius = 3.0f;
    return true;
}

static void shutdown(void) 
{
    sx_free(the_core->alloc(RIZZ_MEMID_GAME), g_simulation.boids);
}

static void update_job_cb(int start, int end, int thrd_index, void* user)
{
    float dt = the_core->delta_time();
    for (int i = start; i < end; i++) {
        update_boid(i, dt);
    }
}

static void update(float dt)
{
    float move_speed = 3.0f;
    if (the_app->key_pressed(RIZZ_APP_KEYCODE_LEFT_SHIFT) ||
        the_app->key_pressed(RIZZ_APP_KEYCODE_RIGHT_SHIFT))
        move_speed = 10.0f;

    // update keyboard movement: WASD first-person
    if (the_app->key_pressed(RIZZ_APP_KEYCODE_A) || the_app->key_pressed(RIZZ_APP_KEYCODE_LEFT)) {
        the_camera->fps_strafe(&g_simulation.cam, -move_speed * dt);
    }

    if (the_app->key_pressed(RIZZ_APP_KEYCODE_D) || the_app->key_pressed(RIZZ_APP_KEYCODE_RIGHT)) {
        the_camera->fps_strafe(&g_simulation.cam, move_speed * dt);
    }

    if (the_app->key_pressed(RIZZ_APP_KEYCODE_W) || the_app->key_pressed(RIZZ_APP_KEYCODE_UP)) {
        the_camera->fps_forward(&g_simulation.cam, move_speed * dt);
    }

    if (the_app->key_pressed(RIZZ_APP_KEYCODE_S) || the_app->key_pressed(RIZZ_APP_KEYCODE_DOWN)) {
        the_camera->fps_forward(&g_simulation.cam, -move_speed * dt);
    }

    if (the_imgui->Begin("Boids", NULL, 0)) {
        the_imgui->Checkbox("Use JobSystem", &g_simulation.use_jobs);
    }
    the_imgui->End();

    static float time = 0.0f;
    time += dt;
    g_simulation.obstacle_origin.x = sx_sin(time) * 1.5f;
    g_simulation.obstacle_origin.y = sx_cos(time) * 1.5f;
    g_simulation.obstacle_origin.z = sx_sin(time * 0.25f) * 1.5f;

    // simulate boids
    if (g_simulation.use_jobs) {
        sx_job_t job =
            the_core->job_dispatch(NUM_BOIDS, update_job_cb, NULL, SX_JOB_PRIORITY_HIGH, 0);
        the_core->job_wait_and_del(job);
    } else {
        for (int i = 0; i < g_simulation.boids->count; i++) {
            update_boid(i, dt);
        }
    }

    show_debugmenu(the_imgui, the_core);
}

static void render(void)
{
    sg_pass_action pass_action = {
        .colors[0] = { SG_ACTION_CLEAR, { 0.0f, 0.0f, 0.0f, 0.0f } },
        .depth = { SG_ACTION_CLEAR, 1.0f },
    };
    the_gfx->staged.begin(g_simulation.stage);
    the_gfx->staged.begin_default_pass(&pass_action, the_app->width(), the_app->height());

    sx_mat4 proj, view;
    the_camera->perspective_mat(&g_simulation.cam.cam, &proj);
    the_camera->view_mat(&g_simulation.cam.cam, &view);
    sx_mat4 viewproj = sx_mat4_mul(&proj, &view);

    const sx_alloc* talloc = the_core->tmp_alloc_push();
    sx_scope(the_core->tmp_alloc_pop())
    {
        const int count = g_simulation.boids->count;
        sx_box* boxes = sx_malloc(talloc, sizeof(sx_box) * count);
        for (int i = 0; i < count; i++) {
            sx_vec3 nvel = g_simulation.boids->vel[i];
            sx_vec3 zz = sx_vec3_norm(nvel);
            sx_vec3 xx = sx_vec3_norm(sx_vec3_cross(zz, SX_VEC3_UNITY));
            sx_vec3 yy = sx_vec3_cross(xx, zz);

            sx_mat3 m = sx_mat3v(xx, sx_vec3_mulf(yy, -1.0f), zz);

            boxes[i].tx = sx_tx3d_set(g_simulation.boids->pos[i], m);
            boxes[i].e = sx_vec3f(.05f, .05f, .2f);
        }

        the_3d->debug.draw_boxes(boxes, NUM_BOIDS, &viewproj, RIZZ_3D_DEBUG_MAPTYPE_WHITE, g_simulation.boids->col);
        the_3d->debug.draw_sphere(g_simulation.obstacle_origin, g_simulation.obstacle_radius,
                                  &viewproj, RIZZ_3D_DEBUG_MAPTYPE_WHITE, sx_color4f(1.0f, 0.0f, 0.0f, 0.5f));
    }

    sx_box wall_box;
    wall_box.e = sx_vec3f(WALL_SIZE, WALL_SIZE, WALL_SIZE);
    wall_box.tx = sx_tx3d_ident();


    the_3d->debug.draw_box(&wall_box, &viewproj, RIZZ_3D_DEBUG_MAPTYPE_WHITE, sx_color4f(0.0f, 0.5f, 1.0f, 0.3f));
    the_gfx->staged.end_pass();
    the_gfx->staged.end();
}

rizz_plugin_decl_event_handler(boids, e)
{
    static bool mouse_down = false;
    float dt = (float)sx_tm_sec(the_core->delta_tick());
    const float rotate_speed = 5.0f;
    static sx_vec2 last_mouse;

    switch (e->type) {
    case RIZZ_APP_EVENTTYPE_SUSPENDED:
        break;
    case RIZZ_APP_EVENTTYPE_RESTORED:
        break;
    case RIZZ_APP_EVENTTYPE_MOUSE_DOWN:
        if (!mouse_down) {
            the_app->mouse_capture();
        }
        mouse_down = true;
        last_mouse = sx_vec2f(e->mouse_x, e->mouse_y);
        break;
    case RIZZ_APP_EVENTTYPE_MOUSE_UP:
        if (mouse_down) {
            the_app->mouse_release();
        }
    case RIZZ_APP_EVENTTYPE_MOUSE_LEAVE:
        mouse_down = false;
        break;
    case RIZZ_APP_EVENTTYPE_MOUSE_MOVE:
        if (mouse_down) {
            float dx = sx_torad(e->mouse_x - last_mouse.x) * rotate_speed * dt;
            float dy = sx_torad(e->mouse_y - last_mouse.y) * rotate_speed * dt;
            last_mouse = sx_vec2f(e->mouse_x, e->mouse_y);

            if (!the_imguix->gizmo.is_using() && !the_imguix->is_capturing_mouse()) {
                the_camera->fps_pitch(&g_simulation.cam, dy);
                the_camera->fps_yaw(&g_simulation.cam, dx);
            }
        }
        break;
    default:
        break;
    }
}

rizz_plugin_decl_main(boids, plugin, e)
{
    switch (e) {
    case RIZZ_PLUGIN_EVENT_STEP:
        update(the_core->delta_time());
        render();
        break;

    case RIZZ_PLUGIN_EVENT_INIT:
        // runs only once for application. Retreive needed APIs
        the_core = (rizz_api_core*)plugin->api->get_api(RIZZ_API_CORE, 0);
        the_gfx = (rizz_api_gfx*)plugin->api->get_api(RIZZ_API_GFX, 0);
        the_app = (rizz_api_app*)plugin->api->get_api(RIZZ_API_APP, 0);
        the_vfs = (rizz_api_vfs*)plugin->api->get_api(RIZZ_API_VFS, 0);
        the_asset = (rizz_api_asset*)plugin->api->get_api(RIZZ_API_ASSET, 0);
        the_camera = (rizz_api_camera*)plugin->api->get_api(RIZZ_API_CAMERA, 0);
        the_imgui = (rizz_api_imgui*)plugin->api->get_api_byname("imgui", 0);
        the_imguix = (rizz_api_imgui_extra*)plugin->api->get_api_byname("imgui_extra", 0);
        the_3d = (rizz_api_3d*)plugin->api->get_api_byname("3dtools", 0);
        the_utility = (rizz_api_utility*)plugin->api->get_api_byname("utility", 0);

        init();
        break;

    case RIZZ_PLUGIN_EVENT_LOAD:
        break;

    case RIZZ_PLUGIN_EVENT_UNLOAD:
        break;

    case RIZZ_PLUGIN_EVENT_SHUTDOWN:
        shutdown();
        break;
    }

    return 0;
}

rizz_game_decl_config(conf)
{
    conf->app_name = "";
    conf->app_version = 1000;
    conf->app_title = "boids";
    conf->app_flags |= RIZZ_APP_FLAG_HIGHDPI;
    conf->log_level = RIZZ_LOG_LEVEL_DEBUG;
    conf->window_width = EXAMPLES_DEFAULT_WIDTH;
    conf->window_height = EXAMPLES_DEFAULT_HEIGHT;
    conf->multisample_count = 4;
    conf->swap_interval = RIZZ_APP_SWAP_INTERVAL_NOSYNC;
    conf->plugins[0] = "imgui";
    conf->plugins[1] = "3dtools";
    conf->plugins[2] = "utility";
}