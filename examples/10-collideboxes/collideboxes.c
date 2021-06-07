#include "rizz/rizz.h"
#include "rizz/imgui.h"
#include "rizz/3dtools.h"
#include "rizz/collision.h"
#include "rizz/imgui-extra.h"

#include "sx/timer.h"
#include "sx/rng.h"
#include "sx/array.h"
#include "sx/math-vec.h"

#include "../common.h"

#define MAP_SIZE_X 200.0f
#define MAP_SIZE_Y 200.0f
#define NUM_SHAPES 2000

RIZZ_STATE static rizz_api_core* the_core;
RIZZ_STATE static rizz_api_gfx* the_gfx;
RIZZ_STATE static rizz_api_app* the_app;
RIZZ_STATE static rizz_api_camera* the_cam;
RIZZ_STATE static rizz_api_imgui* the_imgui;
RIZZ_STATE static rizz_api_imgui_extra* the_imguix;
RIZZ_STATE static rizz_api_3d* the_3d;
RIZZ_STATE static rizz_api_plugin* the_plugin;
RIZZ_STATE static rizz_api_coll* the_coll;

typedef struct entity_t {
    sx_box shape;
    sx_aabb aabb;
    sx_vec3 pos;
    sx_vec3 p1;
    sx_vec3 p2;
    float speed;
    float tm;
    int64_t col_frame;
    int64_t raycast_frame;
} entity_t;

typedef struct collideboxes_state_t {
    rizz_coll_context* ctx;
    sx_rng rng;
    rizz_gfx_stage main_stage;
    rizz_camera_fps cam;
    rizz_texture checker_tex;
    float cell_size;
    entity_t ents[NUM_SHAPES];
    sx_mat4 mat;
    float raycast_len;
    rizz_coll_debug_collision_mode collision_vis_mode;
    rizz_coll_debug_raycast_mode raycast_vis_mode;
    bool show_grid;
    bool pause;
    bool show_collision_debugger;
    bool show_raycast_debugger;
} collideboxes_state;

static collideboxes_state g_coll;

static void make_shapes(rizz_coll_context* ctx)
{
    sx_box boxes[NUM_SHAPES];
    uint64_t ents[NUM_SHAPES];
    uint32_t masks[NUM_SHAPES];
    sx_tx3d transforms[NUM_SHAPES];

    for (int i = 0; i < NUM_SHAPES; i++) {
        entity_t* ent = &g_coll.ents[i];

        sx_vec3 extents = sx_vec3f(
            sx_rng_genf(&g_coll.rng) + 0.2f, 
            sx_rng_genf(&g_coll.rng) + 0.2f,
            sx_rng_genf(&g_coll.rng) + 0.2f);

        sx_vec3 pos = sx_vec3f((sx_rng_genf(&g_coll.rng)*2.0f - 1.0f)*MAP_SIZE_X*0.5f, 
                               (sx_rng_genf(&g_coll.rng)*2.0f - 1.0f)*MAP_SIZE_Y*0.5f,
                               extents.z*0.5f);
        ent->shape = sx_box_set(sx_tx3d_setf(0, 0, 0, 0, 0, sx_rng_genf(&g_coll.rng)*SX_PI2), extents);

        // choose two points in a random range radius for entity to move between
        float move_range = sx_rng_genf(&g_coll.rng)*8.0f;
        float theta = sx_rng_genf(&g_coll.rng)*SX_PI2;

        ent->pos = pos;
        ent->p1 = sx_vec3_add(pos, sx_vec3f(move_range*sx_cos(theta), move_range*sx_sin(theta), 0));
        ent->p2 = sx_vec3_add(pos, sx_vec3f(move_range*sx_cos(SX_PI - theta), move_range*sx_sin(SX_PI - theta), 0));

        ent->aabb = sx_aabb_from_box(&ent->shape);
        ent->speed = sx_rng_genf(&g_coll.rng);

        boxes[i] = ent->shape;
        ents[i] = i;
        masks[i] = 0xffffffff;
        transforms[i] = sx_tx3d_set(pos, sx_mat3_ident());
    }

    the_coll->add_boxes(ctx, boxes, ents, masks, transforms, NUM_SHAPES);
}

static bool init(void)
{
    sx_tm_init();
    sx_rng_seed_time(&g_coll.rng);
    g_coll.main_stage = the_gfx->stage_register("main", (rizz_gfx_stage){.id = 0});
    sx_assert(g_coll.main_stage.id);

    the_3d->debug.set_max_instances(2000);

    // camera
    // projection: setup for perspective
    // view: Z-UP Y-Forward (like blender)
    sx_vec2 screen_size;
    the_app->window_size(&screen_size);
    const float view_width = screen_size.x;
    const float view_height = screen_size.y;
    the_cam->fps_init(&g_coll.cam, 50.0f, sx_rectwh(0, 0, view_width, view_height), 0.1f, 500.0f);
    the_cam->fps_lookat(&g_coll.cam, sx_vec3f(0, -3.0f, 3.0f), SX_VEC3_ZERO, SX_VEC3_UNITZ);

    const sx_color checker_colors[] = {
        sx_color4u(200, 200, 200, 255), 
        sx_color4u(255, 255, 255, 255)
    };
    g_coll.checker_tex = the_gfx->texture_create_checker(8, 128, checker_colors);
    g_coll.show_grid = true;    
    g_coll.raycast_len = 20.0f;
    g_coll.cell_size = 4.0f;
    g_coll.mat = sx_mat4_ident();

    g_coll.ctx = the_coll->create_context(MAP_SIZE_X, MAP_SIZE_Y, g_coll.cell_size, the_core->heap_alloc());
    make_shapes(g_coll.ctx);

    return true;
}

static void shutdown(void)
{
    the_coll->destroy_context(g_coll.ctx);
}

static void update_cam_movement(float dt)
{
    if (the_imguix->is_capturing_keyboard()) {
        return;
    }

    float move_speed = 3.0f;
    if (the_app->key_pressed(RIZZ_APP_KEYCODE_RIGHT_SHIFT) || the_app->key_pressed(RIZZ_APP_KEYCODE_LEFT_SHIFT)) {
        move_speed *= 4.0f;
    }

    // update keyboard movement: WASD first-person
    if (the_app->key_pressed(RIZZ_APP_KEYCODE_A) || the_app->key_pressed(RIZZ_APP_KEYCODE_LEFT)) {
        the_cam->fps_strafe(&g_coll.cam, -move_speed*dt);
    }

    if (the_app->key_pressed(RIZZ_APP_KEYCODE_D) || the_app->key_pressed(RIZZ_APP_KEYCODE_RIGHT)) {
        the_cam->fps_strafe(&g_coll.cam, move_speed*dt);
    }

    if (the_app->key_pressed(RIZZ_APP_KEYCODE_W) || the_app->key_pressed(RIZZ_APP_KEYCODE_UP)) {
        the_cam->fps_forward(&g_coll.cam, move_speed*dt);
    }

    if (the_app->key_pressed(RIZZ_APP_KEYCODE_S) || the_app->key_pressed(RIZZ_APP_KEYCODE_DOWN)) {
        the_cam->fps_forward(&g_coll.cam, -move_speed*dt);
    }
}

static void update(float dt)
{
    update_cam_movement(dt);

    const sx_alloc* tmp_alloc = the_core->tmp_alloc_push();
    rizz_profile(CollisionUpdate) {
    sx_scope(the_core->tmp_alloc_pop()) {
        // move shapes around
        int64_t frame = the_core->frame_index();
        int num_pairs;
        float detect_time, update_time;
        {
            uint64_t ents[NUM_SHAPES];
            sx_tx3d transforms[NUM_SHAPES];
            for (int i = 0; i < NUM_SHAPES; i++) {
                entity_t* ent = &g_coll.ents[i];
                ent->tm += dt;
                float t = sx_sin(ent->tm)*0.5f + 0.5f;
                ents[i] = i;
                sx_vec3 pos = sx_vec3_lerp(ent->p1, ent->p2, t);
                transforms[i] = sx_tx3d_set(pos, sx_mat3_ident());
                ent->pos = pos;
            }

            uint64_t t0 = sx_tm_now();
            the_coll->update_transforms(g_coll.ctx, ents, NUM_SHAPES, transforms);

            uint64_t t1 = sx_tm_now();
            update_time = (float)sx_tm_ms(sx_tm_diff(t1, t0));

            rizz_coll_pair* pairs = the_coll->detect(g_coll.ctx, tmp_alloc);
            sx_assert_always(pairs);
            for (int i = 0, ic = sx_array_count(pairs); i < ic; i++) {
                rizz_coll_pair pair = pairs[i];
                g_coll.ents[pair.ent1].col_frame = frame;
                g_coll.ents[pair.ent2].col_frame = frame;
            }

            detect_time = (float)sx_tm_ms(sx_tm_diff(sx_tm_now(), t1));
            num_pairs = sx_array_count(pairs);
        }

        if (g_coll.show_raycast_debugger) {
            sx_vec3 ray_pos = g_coll.cam.cam.pos;
            sx_vec3 ray_dir = g_coll.cam.cam.forward;

            rizz_coll_ray ray = rizz_coll_ray_set(ray_pos, ray_dir, g_coll.raycast_len);
            rizz_coll_rayhit* hits = the_coll->query_ray(g_coll.ctx, ray, 0xffffffff, tmp_alloc);
            sx_assert_always(hits);
            for (int i = 0, ic = sx_array_count(hits); i < ic; i++) {
                rizz_coll_rayhit hit = hits[i];
                g_coll.ents[hit.ent].raycast_frame = frame;
            }
        }
	
	    show_debugmenu(the_imgui, the_core);	

        if (the_imgui->Begin("collision", NULL, 0)) {
            if (the_imgui->Checkbox("Visualize Collision (F3)", &g_coll.show_collision_debugger)) {
                if (g_coll.show_collision_debugger) {
                    g_coll.show_raycast_debugger = false;
                }
            }
            const char* items[] = {
                "Collisions",
                "Collision Heatmap",
                "Entity Heatmap"
            };
            the_imgui->Combo_Str_arr("Collision vis Mode", (int*)&g_coll.collision_vis_mode, items, 3, -1);
            the_imgui->SliderFloat("Raycast len", &g_coll.raycast_len, 1.0f, 100.0f, "%.0f", 0);
            if (the_imgui->Checkbox("Debug Raycast", &g_coll.show_raycast_debugger)) {
                if (g_coll.show_raycast_debugger) {
                    g_coll.show_collision_debugger = false;
                }
            }
            const char* items2[] = {
                "Rayhits",
                "Rayhit Heatmap",
                "Raymarch Heatmap"
            };
            the_imgui->Combo_Str_arr("Raycast vis Mode", (int*)&g_coll.raycast_vis_mode, items2, 3, -1);        
            the_imgui->LabelText("Collisions", "%d", num_pairs);
        } 
        the_imgui->LabelText("Update time", "%.3f ms", update_time);
        the_imgui->LabelText("Detection time", "%.3f ms", detect_time);
        the_imgui->LabelText("Ft", "%.3f", the_core->delta_time()*1000.0f);
        the_imgui->End();

        if (g_coll.show_collision_debugger) {
            the_coll->debug_collisions(g_coll.ctx, 0.95f, g_coll.collision_vis_mode, 10.0f);
        }
        if (g_coll.show_raycast_debugger) {
            the_coll->debug_raycast(g_coll.ctx, 0.6f, g_coll.raycast_vis_mode, 1.0f);
            // crossair
            ImDrawList* draw_list = the_imguix->begin_fullscreen_draw("crossair");
            sx_vec2 size;
            the_app->window_size(&size);
            sx_vec2 center = sx_vec2_mulf(size, 0.5f);
            the_imgui->ImDrawList_AddCircle(draw_list, center, 5.0f, SX_COLOR_YELLOW.n, 12, 4.0f);
        }
    } // scope
    } // profile

}

static void render(void)
{
    sg_pass_action pass_action = { .colors[0] = { SG_ACTION_CLEAR, {0.25f, 0.5f, 0.75f, 1.0f} },
                                   .depth = { SG_ACTION_CLEAR, 1.0f } };
    the_gfx->staged.begin(g_coll.main_stage);
    the_gfx->staged.begin_default_pass(&pass_action, the_app->width(), the_app->height());

    sx_mat4 proj, view;
    the_cam->perspective_mat(&g_coll.cam.cam, &proj);
    the_cam->view_mat(&g_coll.cam.cam, &view);
    sx_mat4 viewproj = sx_mat4_mul(&proj, &view);

    if (g_coll.show_grid) {
        the_3d->debug.grid_xyplane_cam(g_coll.cell_size, g_coll.cell_size*5.0f, 50.0f, &g_coll.cam.cam, &viewproj);
    }

    sx_box boxes[NUM_SHAPES];
    sx_color colors[NUM_SHAPES];
    int64_t frame_idx = the_core->frame_index();
    sx_tx3d tx;
    tx.rot = sx_mat3_ident();
    if (!g_coll.show_raycast_debugger) {
        for (int i = 0; i < NUM_SHAPES; i++) {
            tx.pos = g_coll.ents[i].pos;
            boxes[i] = sx_box_set(sx_tx3d_mul(&tx, &g_coll.ents[i].shape.tx), g_coll.ents[i].shape.e);
            colors[i] = g_coll.ents[i].col_frame == frame_idx ? SX_COLOR_RED : SX_COLOR_WHITE;
        }
    } else {
        for (int i = 0; i < NUM_SHAPES; i++) {
            tx.pos = g_coll.ents[i].pos;
            boxes[i] = sx_box_set(sx_tx3d_mul(&tx, &g_coll.ents[i].shape.tx), g_coll.ents[i].shape.e);
            colors[i] = g_coll.ents[i].raycast_frame == frame_idx ? SX_COLOR_RED : SX_COLOR_WHITE;
        }
    }
    
    sx_box* box = &boxes[NUM_SHAPES-1];
    tx = sx_tx3d_set(sx_vec3f(0, 0.0f, 0), sx_mat3_rotate(sx_torad(60.0f)));
    box->tx.pos = sx_vec3f(-5.0f, -5.0f, 0);
    box->tx.rot = sx_mat3_rotate(-sx_torad(80.0f));
    box->tx = sx_tx3d_mul(&tx, &box->tx);
    
    the_3d->debug.draw_boxes(boxes, NUM_SHAPES, &viewproj, RIZZ_3D_DEBUG_MAPTYPE_CHECKER, colors);

    the_gfx->staged.end_pass();
    the_gfx->staged.end();
}

rizz_plugin_decl_main(collideboxes, plugin, e)
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
        the_cam = (rizz_api_camera*)plugin->api->get_api(RIZZ_API_CAMERA, 0);
        the_imgui = (rizz_api_imgui*)plugin->api->get_api_byname("imgui", 0);
        the_imguix = (rizz_api_imgui_extra*)plugin->api->get_api_byname("imgui_extra", 0);
        the_3d = (rizz_api_3d*)plugin->api->get_api_byname("3dtools", 0);
        the_coll = (rizz_api_coll*)plugin->api->get_api_byname("collision", 0);

        the_plugin = plugin->api;

        if (!init()) {
            return -1;
        }
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

rizz_plugin_decl_event_handler(collideboxes, e)
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
                the_cam->fps_pitch(&g_coll.cam, dy);
                the_cam->fps_yaw(&g_coll.cam, dx);
            }
        }
        break;

    case RIZZ_APP_EVENTTYPE_KEY_DOWN:
        if (e->key_code == RIZZ_APP_KEYCODE_SPACE) {
            g_coll.pause = !g_coll.pause;
        } else if (e->key_code == RIZZ_APP_KEYCODE_F3) {
            g_coll.show_collision_debugger = !g_coll.show_collision_debugger;
        }
        break;

    case RIZZ_APP_EVENTTYPE_UPDATE_APIS:
        the_imgui = the_plugin->get_api_byname("imgui", 0);
        the_imguix = the_plugin->get_api_byname("imgui_extra", 0);
        the_3d = (rizz_api_3d*)the_plugin->get_api_byname("3dtools", 0);
        break;
    default:
        break;
    }
}

rizz_game_decl_config(conf)
{
    conf->app_name = "collideboxes";
    conf->app_version = 1000;
    conf->app_title = "Collision example";
    conf->app_flags |= RIZZ_APP_FLAG_HIGHDPI;
    conf->log_level = RIZZ_LOG_LEVEL_DEBUG;
    conf->window_width = EXAMPLES_DEFAULT_WIDTH;
    conf->window_height = EXAMPLES_DEFAULT_HEIGHT;
    conf->multisample_count = 4;
    conf->swap_interval = 1;
    conf->plugins[0] = "imgui";
    conf->plugins[1] = "3dtools";
    conf->plugins[2] = "collision";
}

