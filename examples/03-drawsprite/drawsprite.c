#include "sx/os.h"
#include "sx/string.h"
#include "sx/timer.h"
#include "sx/math-vec.h"

#include "rizz/2dtools.h"
#include "rizz/imgui-extra.h"
#include "rizz/imgui.h"
#include "rizz/rizz.h"

#include "../common.h"

#define MAX_VERTICES 1000
#define MAX_INDICES 2000
#define NUM_SPRITES 6
#define SPRITE_WIDTH 3.5f

RIZZ_STATE static rizz_api_core* the_core;
RIZZ_STATE static rizz_api_gfx* the_gfx;
RIZZ_STATE static rizz_api_app* the_app;
RIZZ_STATE static rizz_api_imgui* the_imgui;
RIZZ_STATE static rizz_api_asset* the_asset;
RIZZ_STATE static rizz_api_camera* the_camera;
RIZZ_STATE static rizz_api_vfs* the_vfs;
RIZZ_STATE static rizz_api_2d* the_2d;

typedef struct {
    sx_mat4 vp;
    sx_vec4 motion;
} drawsprite_params;

typedef struct {
    sx_vec2 pos;
    sx_vec2 uv;
    sx_vec4 transform;    // (x,y: pos) (z: rotation) (w: scale)
    sx_color color;
    sx_vec3 bc;
} drawsprite_vertex;

static rizz_vertex_layout k_vertex_layout = {
    .attrs[0] = { .semantic = "POSITION", .offset = offsetof(drawsprite_vertex, pos) },
    .attrs[1] = { .semantic = "TEXCOORD", .offset = offsetof(drawsprite_vertex, uv) },
    .attrs[2] = { .semantic = "TEXCOORD",
                  .semantic_idx = 1,
                  .offset = offsetof(drawsprite_vertex, transform) }
};

static rizz_vertex_layout k_vertex_layout_wire = {
    .attrs[0] = { .semantic = "POSITION", .offset = offsetof(drawsprite_vertex, pos) },
    .attrs[1] = { .semantic = "TEXCOORD", .offset = offsetof(drawsprite_vertex, uv) },
    .attrs[2] = { .semantic = "TEXCOORD",
                  .semantic_idx = 1,
                  .offset = offsetof(drawsprite_vertex, transform) },
    .attrs[3] = { .semantic = "TEXCOORD",
                  .semantic_idx = 2,
                  .offset = offsetof(drawsprite_vertex, bc) }
};

typedef struct {
    rizz_gfx_stage stage;
    sg_pipeline pip;
    sg_pipeline pip_wire;
    rizz_asset atlas;
    rizz_asset shader;
    rizz_asset shader_wire;
    sg_buffer vbuff;
    sg_buffer ibuff;
    rizz_camera_fps cam;
    rizz_sprite sprites[NUM_SPRITES];
    rizz_asset font;
    bool wireframe;
    bool custom;
} drawsprite_state;

RIZZ_STATE static drawsprite_state g_ds;

static bool init()
{
#if SX_PLATFORM_ANDROID || SX_PLATFORM_IOS
    the_vfs->mount_mobile_assets("/assets");
#else
    // mount `/asset` directory
    char asset_dir[RIZZ_MAX_PATH];
    sx_os_path_join(asset_dir, sizeof(asset_dir), EXAMPLES_ROOT, "assets");    // "/examples/assets"
    the_vfs->mount(asset_dir, "/assets");
#endif

    // register main graphics stage.
    // at least one stage should be registered if you want to draw anything
    g_ds.stage = the_gfx->stage_register("main", (rizz_gfx_stage){ .id = 0 });
    sx_assert(g_ds.stage.id);
    
    // load font
    rizz_font_load_params fparams = { 0 };
    g_ds.font = the_asset->load("font", "/assets/fonts/sponge_bob.ttf", &fparams, 0, NULL, 0);

    // sprite device objects
    g_ds.vbuff =
        the_gfx->make_buffer(&(sg_buffer_desc){ .usage = SG_USAGE_STREAM,
                                                .type = SG_BUFFERTYPE_VERTEXBUFFER,
                                                .size = sizeof(drawsprite_vertex) * MAX_VERTICES });

    g_ds.ibuff = the_gfx->make_buffer(&(sg_buffer_desc){ .usage = SG_USAGE_STREAM,
                                                         .type = SG_BUFFERTYPE_INDEXBUFFER,
                                                         .size = sizeof(uint16_t) * MAX_INDICES });

    char shader_path[RIZZ_MAX_PATH];
    g_ds.shader = the_asset->load("shader",
        ex_shader_path(shader_path, sizeof(shader_path), "/assets/shaders", "drawsprite.sgs"), NULL, 0, NULL, 0);
    g_ds.shader_wire = the_asset->load("shader",
        ex_shader_path(shader_path, sizeof(shader_path), "/assets/shaders", "drawsprite_wire.sgs"), NULL, 0, NULL, 0);

    // pipeline
    sg_pipeline_desc pip_desc = { .layout.buffers[0].stride = sizeof(drawsprite_vertex),
                                  .index_type = SG_INDEXTYPE_UINT16,
                                  .rasterizer = { .cull_mode = SG_CULLMODE_BACK },
                                  .blend = { .enabled = true,
                                             .src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA,
                                             .dst_factor_rgb =
                                                 SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA } };
    g_ds.pip = the_gfx->make_pipeline(the_gfx->shader_bindto_pipeline(
        the_gfx->shader_get(g_ds.shader), &pip_desc, &k_vertex_layout));

    // pipeline
    sg_pipeline_desc pip_desc_wire = {
        .layout.buffers[0].stride = sizeof(drawsprite_vertex),
        .rasterizer = { .cull_mode = SG_CULLMODE_BACK },
        .blend = { .enabled = true,
                   .src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA,
                   .dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA },
    };
    g_ds.pip_wire = the_gfx->make_pipeline(the_gfx->shader_bindto_pipeline(
        the_gfx->shader_get(g_ds.shader_wire), &pip_desc_wire, &k_vertex_layout_wire));

    // camera
    // projection: setup for ortho, total-width = 10 units
    // view: Y-UP
    sx_vec2 screen_size;
    the_app->window_size(&screen_size);
    const float view_width = 5.0f;
    const float view_height = screen_size.y * view_width / screen_size.x;
    the_camera->fps_init(&g_ds.cam, 50.0f,
                         sx_rectf(-view_width, -view_height, view_width, view_height), -5.0f, 5.0f);
    the_camera->fps_lookat(&g_ds.cam, sx_vec3f(0, 0.0f, 1.0), SX_VEC3_ZERO, SX_VEC3_UNITY);

    // sprites and atlases
    rizz_atlas_load_params aparams = { .min_filter = SG_FILTER_LINEAR,
                                       .mag_filter = SG_FILTER_LINEAR };
    g_ds.atlas = the_asset->load("atlas", "/assets/textures/handicraft.json", &aparams,
                                 RIZZ_ASSET_LOAD_FLAG_WAIT_ON_LOAD, NULL, 0);

    for (int i = 0; i < NUM_SPRITES; i++) {
        char name[32];
        sx_snprintf(name, sizeof(name), "test/handicraft_%d.png", i + 1);
        g_ds.sprites[i] = the_2d->sprite.create(&(rizz_sprite_desc){ .name = name,
                                                                  .atlas = g_ds.atlas,
                                                                  .size = sx_vec2f(SPRITE_WIDTH, 0),
                                                                  .color = sx_colorn(0xffffffff) });
    }

    return true;
}

static void shutdown()
{
    for (int i = 0; i < NUM_SPRITES; i++) {
        if (g_ds.sprites[i].id) {
            the_2d->sprite.destroy(g_ds.sprites[i]);
        }
    }
    if (g_ds.vbuff.id)
        the_gfx->destroy_buffer(g_ds.vbuff);
    if (g_ds.ibuff.id)
        the_gfx->destroy_buffer(g_ds.ibuff);
    if (g_ds.atlas.id)
        the_asset->unload(g_ds.atlas);
    if (g_ds.shader.id)
        the_asset->unload(g_ds.shader);
    if (g_ds.shader_wire.id)
        the_asset->unload(g_ds.shader_wire);
    if (g_ds.pip_wire.id)
        the_gfx->destroy_pipeline(g_ds.pip_wire);
    if (g_ds.font.id) 
        the_asset->unload(g_ds.font);
}

static void update(float dt) {}

// Custom drawing uses `make_drawdata_batch` API function
// which basically returns vertex-buffer/index-buffer and batch data needed to draw the input
// sprites effieciently.
// As an example, we modify vertices and use custom shader with the draw-data
static void draw_custom(const drawsprite_params* params)
{
    const sx_alloc* tmp_alloc = the_core->tmp_alloc_push();
    sx_scope(the_core->tmp_alloc_pop()) {
        rizz_sprite_drawdata* dd =
            the_2d->sprite.make_drawdata_batch(g_ds.sprites, NUM_SPRITES, tmp_alloc);

        sg_bindings bindings = { .vertex_buffers[0] = g_ds.vbuff };

        // populate new vertex buffer
        if (!g_ds.wireframe) {
            bindings.index_buffer = g_ds.ibuff;
            drawsprite_vertex* verts = sx_malloc(tmp_alloc, sizeof(drawsprite_vertex) * dd->num_verts);
            sx_assert(verts);

            float start_x = -3.0f;
            float start_y = -1.5f;
            for (int i = 0; i < dd->num_sprites; i++) {
                rizz_sprite_drawsprite* dspr = &dd->sprites[i];

                sx_vec4 transform = sx_vec4f(start_x, start_y, 0, 1.0f);
                int end_vertex = dspr->start_vertex + dspr->num_verts;
                for (int v = dspr->start_vertex; v < end_vertex; v++) {
                    verts[v].pos = dd->verts[v].pos;
                    verts[v].uv = dd->verts[v].uv;
                    verts[v].transform = transform;
                    verts[v].color = dd->verts[v].color;
                }
                start_x += sx_rect_width(the_2d->sprite.bounds(g_ds.sprites[i])) * 0.8f;

                if ((i + 1) % 3 == 0) {
                    start_y += 3.0f;
                    start_x = -3.0f;
                }
            }

            the_gfx->staged.update_buffer(g_ds.vbuff, verts, sizeof(drawsprite_vertex) * dd->num_verts);
            the_gfx->staged.update_buffer(g_ds.ibuff, dd->indices, sizeof(uint16_t) * dd->num_indices);
            the_gfx->staged.apply_pipeline(g_ds.pip);
        } else {
            drawsprite_vertex* verts =
                sx_malloc(tmp_alloc, sizeof(drawsprite_vertex) * dd->num_indices);
            sx_assert(verts);
            const sx_vec3 bcs[] = { { { 1.0f, 0, 0 } }, { { 0, 1.0f, 0 } }, { { 0, 0, 1.0f } } };

            float start_x = -3.0f;
            float start_y = -1.5f;
            int vindex = 0;
            for (int i = 0; i < dd->num_sprites; i++) {
                rizz_sprite_drawsprite* dspr = &dd->sprites[i];

                sx_vec4 transform = sx_vec4f(start_x, start_y, 0, 1.0f);
                int end_index = dspr->start_index + dspr->num_indices;
                for (int ii = dspr->start_index; ii < end_index; ii++) {
                    int v = dd->indices[ii];
                    verts[vindex].pos = dd->verts[v].pos;
                    verts[vindex].uv = dd->verts[v].uv;
                    verts[vindex].transform = transform;
                    verts[vindex].color = dd->verts[v].color;
                    verts[vindex].bc = bcs[vindex % 3];
                    vindex++;
                }
                start_x += sx_rect_width(the_2d->sprite.bounds(g_ds.sprites[i])) * 0.8f;

                if ((i + 1) % 3 == 0) {
                    start_y += 3.0f;
                    start_x = -3.0f;
                }
            }

            the_gfx->staged.update_buffer(g_ds.vbuff, verts,
                                          sizeof(drawsprite_vertex) * dd->num_indices);
            the_gfx->staged.apply_pipeline(g_ds.pip_wire);
        }

        bindings.fs_images[0] = the_gfx->texture_get(dd->batches[0].texture)->img;
        the_gfx->staged.apply_bindings(&bindings);

        the_gfx->staged.apply_uniforms(SG_SHADERSTAGE_VS, 0, params, sizeof(*params));

        the_gfx->staged.draw(0, dd->num_indices, 1);
    } // scope
}

static void render()
{
    sg_pass_action pass_action = { .colors[0] = { SG_ACTION_CLEAR, { 0.25f, 0.5f, 0.75f, 1.0f } },
                                   .depth = { SG_ACTION_CLEAR, 1.0f } };

    the_gfx->staged.begin(g_ds.stage);
    the_gfx->staged.begin_default_pass(&pass_action, the_app->width(), the_app->height());

    // draw sprite
    sx_mat4 proj, view;
    the_camera->ortho_mat(&g_ds.cam.cam, &proj);
    the_camera->view_mat(&g_ds.cam.cam, &view);
    sx_mat4 vp = sx_mat4_mul(&proj, &view);

    drawsprite_params params = {
        .vp = vp, .motion = { .x = (float)sx_tm_sec(the_core->elapsed_tick()), .y = 0.5f }
    };

    sx_mat3 mats[NUM_SPRITES];
    float start_x = -3.0f;
    float start_y = -1.5f;

    for (int i = 0; i < NUM_SPRITES; i++) {
        mats[i] = sx_mat3_translate(start_x, start_y);

        start_x += sx_rect_width(the_2d->sprite.bounds(g_ds.sprites[i])) * 0.8f;
        if ((i + 1) % 3 == 0) {
            start_y += 3.0f;
            start_x = -3.0f;
        }
    }

    if (!g_ds.custom) {
        the_2d->sprite.draw_batch(g_ds.sprites, NUM_SPRITES, &vp, mats, NULL);
        if (g_ds.wireframe)
            the_2d->sprite.draw_wireframe_batch(g_ds.sprites, NUM_SPRITES, &vp, mats);
    } else {
        draw_custom(&params);
    }

    // draw sample font
    {
        const rizz_font* font = the_2d->font.get(g_ds.font);
        the_2d->font.push_state(font);
        // note: setup ortho matrix in a way that the Y is reversed (top-left = origin)
        float w = (float)the_app->width();
        float h = (float)the_app->height();
        sx_mat4 vp = sx_mat4_ortho_offcenter(0, h, w, 0, -1.0f, 1.0f, 0, the_gfx->GL_family());

        the_2d->font.set_viewproj_mat(font, &vp);
        the_2d->font.set_size(font, 30.0f);
        rizz_font_vert_metrics metrics = the_2d->font.vert_metrics(font);

        float y = metrics.lineh + 15.0f;
        the_2d->font.draw(font, sx_vec2f(15.0f, y), "DrawSprite Example");

        the_2d->font.push_state(font);
        the_2d->font.set_size(font, 16.0f);
        the_2d->font.draw(font, sx_vec2f(15.0f, y + metrics.lineh), "This text is drawn by font API");
        the_2d->font.pop_state(font);

        the_2d->font.pop_state(font);
    }

    the_gfx->staged.end_pass();
    the_gfx->staged.end();

    // UI
    static bool show_debugger = false;
    show_debugmenu(the_imgui, the_core);

    the_imgui->SetNextWindowContentSize(sx_vec2f(140.0f, 120.0f));
    if (the_imgui->Begin("drawsprite", NULL, 0)) {
        the_imgui->LabelText("Fps", "%.3f", the_core->fps());
        the_imgui->Checkbox("Show Debugger", &show_debugger);
        the_imgui->Checkbox("Wireframe", &g_ds.wireframe);
        the_imgui->Checkbox("Custom Drawing", &g_ds.custom);
    }
    the_imgui->End();

    if (show_debugger) {
        the_2d->sprite.show_debugger(&show_debugger);
    }
}

rizz_plugin_decl_main(drawsprite, plugin, e)
{
    switch (e) {
    case RIZZ_PLUGIN_EVENT_STEP:
        update(the_core->delta_time());
        render();
        break;

    case RIZZ_PLUGIN_EVENT_INIT:
        // runs only once for application. Retreive needed APIs
        the_core = plugin->api->get_api(RIZZ_API_CORE, 0);
        the_gfx = plugin->api->get_api(RIZZ_API_GFX, 0);
        the_app = plugin->api->get_api(RIZZ_API_APP, 0);
        the_vfs = plugin->api->get_api(RIZZ_API_VFS, 0);
        the_asset = plugin->api->get_api(RIZZ_API_ASSET, 0);
        the_camera = plugin->api->get_api(RIZZ_API_CAMERA, 0);

        the_imgui = plugin->api->get_api_byname("imgui", 0);
        the_2d = plugin->api->get_api_byname("2dtools", 0);

        if (!init())
            return -1;
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

rizz_plugin_decl_event_handler(drawsprite, e)
{
    switch (e->type) {
    case RIZZ_APP_EVENTTYPE_SUSPENDED:
        break;
    case RIZZ_APP_EVENTTYPE_RESTORED:
        break;
    case RIZZ_APP_EVENTTYPE_MOUSE_DOWN:
        break;
    case RIZZ_APP_EVENTTYPE_MOUSE_UP:
        break;
    case RIZZ_APP_EVENTTYPE_MOUSE_MOVE:
        break;
    default:
        break;
    }
}

rizz_game_decl_config(conf)
{
    conf->app_name = "drawsprite";
    conf->app_version = 1000;
    conf->app_title = "03 - DrawSprite";
    conf->app_flags |= RIZZ_APP_FLAG_HIGHDPI;
    conf->log_level = RIZZ_LOG_LEVEL_DEBUG;
    conf->window_width = EXAMPLES_DEFAULT_WIDTH;
    conf->window_height = EXAMPLES_DEFAULT_HEIGHT;
    conf->swap_interval = 2;
    conf->plugins[0] = "imgui";
    conf->plugins[1] = "2dtools";
}
