#include "sx/io.h"
#include "sx/math.h"
#include "sx/os.h"
#include "sx/string.h"

#include "rizz/imgui.h"
#include "rizz/imgui-extra.h"
#include "rizz/3dtools.h"
#include "rizz/rizz.h"

#include "../common.h"

#define CGLTF_IMPLEMENTATION
#include "../3rdparty/cgltf/cgltf.h"

RIZZ_STATE static rizz_api_core* the_core;
RIZZ_STATE static rizz_api_gfx* the_gfx;
RIZZ_STATE static rizz_api_app* the_app;
RIZZ_STATE static rizz_api_imgui* the_imgui;
RIZZ_STATE static rizz_api_asset* the_asset;
RIZZ_STATE static rizz_api_camera* the_camera;
RIZZ_STATE static rizz_api_vfs* the_vfs;
RIZZ_STATE static rizz_api_prims3d* the_prims;
RIZZ_STATE static rizz_api_imgui_extra* the_imguix;

typedef struct {
    rizz_gfx_stage stage;
    rizz_camera_fps cam;
} draw3d_state;

RIZZ_STATE static draw3d_state g_draw3d;

typedef struct rizz_material_texture {
    rizz_asset tex_asset;
    int array_index;
} rizz_material_texture;

typedef struct rizz_material_metallic_roughness {
    rizz_material_texture base_color_tex;
    rizz_material_texture metallic_roughness_tex;
    
    sx_vec4 base_color_factor;
    float metallic_factor;
    float roughness_factor;
} rizz_material_metallic_roughness;

typedef struct rizz_material_specular_glossiness {
    rizz_material_texture diffuse_tex;
    rizz_material_texture specular_glossiness_tex;

    sx_vec4 diffuse_factor;
    sx_vec3 specular_factor;
    float glossiness_factor;
} rizz_material_specular_glossiness;

typedef struct rizz_meterial_clearcoat {
    rizz_asset clearcoat_tex;
    rizz_asset clearcoat_roughness_tex;
    rizz_asset clearcoat_normal_tex;

    float clearcoat_factor;
    float clearcoat_roughness_factor;
} rizz_material_clearcoat;

typedef enum rizz_material_alpha_mode {
    RIZZ_MATERIAL_ALPHAMODE_OPAQUE = 0,
    RIZZ_MATERIAL_ALPHAMODE_MASK,
    RIZZ_MATERIAL_ALPHAMODE_BLEND
} rizz_material_alpha_mode;

typedef struct rizz_material_data {
    char name[32];
    bool has_metal_roughness;
    bool has_specular_glossiness;
    bool has_clearcoat;
    bool _reserved1;
    rizz_material_metallic_roughness pbr_metallic_roughness;
    rizz_material_specular_glossiness pbr_speuclar_glossiness;
    rizz_material_clearcoat clearcoat;
    rizz_material_texture normal_tex;
    rizz_material_texture occlusion_tex;
    rizz_material_texture emissive_tex;
    sx_vec4 emissive_factor;
    rizz_material_alpha_mode alpha_mode;
    float alpha_cutoff;
    bool double_sided;
    bool unlit;
} rizz_material_data;

typedef struct { uint32_t id; } rizz_material;

// mandate:
//  - instance-able
//  - optionally store gpu buffers for immediate rendering
//  - joinable (texture-arrays + )
//  - editable, including instances (dynamic/stream buffers)
//  - joints and skeleton
//  - skinning
//  - wireframe (bc coords)
//  - compression (quantize)
//  - multi-stream (base/skin/tangent/extra)
//      - gbuffer: pos+normal+color+skin+tangent+etc..
//      - gbuffer (def-light): position+normal+tangent
//      - shadows: position (+skin)
//      - light: texcoord + normal
//  - instancing optimization (some kind of hash?)
//  - we can pass a vertex layout and model data will be matched by that ! 
//      - debugging: we need to see all instances of a mesh in the scene
typedef struct rizz_model_submesh {
    int start_index;
    int num_indices;
    rizz_material mtl;
} rizz_model_submesh;

typedef enum rizz_model_vertex_attribute {
    RIZZ_MODEL_VERTEXATTRIBUTE_POSITION = 0,
    RIZZ_MODEL_VERTEXATTRIBUTE_NORMAL,
    RIZZ_MODEL_VERTEXATTRIBUTE_TANGENT,
    RIZZ_MODEL_VERTEXATTRIBUTE_JOINTS,
    RIZZ_MODEL_VERTEXATTRIBUTE_WEIGHTS,
    RIZZ_MODEL_VERTEXATTRIBUTE_COLOR0,
    RIZZ_MODEL_VERTEXATTRIBUTE_COLOR1,
    RIZZ_MODEL_VERTEXATTRIBUTE_COLOR2,
    RIZZ_MODEL_VERTEXATTRIBUTE_COLOR3,
    RIZZ_MODEL_VERTEXATTRIBUTE_TEXCOORD0,
    RIZZ_MODEL_VERTEXATTRIBUTE_TEXCOORD1,
    RIZZ_MODEL_VERTEXATTRIBUTE_TEXCOORD2,
    RIZZ_MODEL_VERTEXATTRIBUTE_TEXCOORD3,
    _RIZZ_MODEL_VERTEXATTRIBUTE_COUNT
} rizz_model_vertex_attribute;

// mesh contains the main geometry for each node of the model
// it includes multiple "submeshes". a submesh is part of the geometry with differnt material
// so, a mesh can contain multiple materials within itself
// vertex buffers are another topic that needs to be discussed here:
//      when loading a model, you should define the whole vertex-layout that your render pipeline uses
//      so for example, a renderer may need to layout it's vertex data like this:
//          - buffer #1: position
//          - buffer #2: normal/tangent
//          - buffer #3: texcoord
//          - buffer #4: joints/weights
//      4 buffers will be reserved (num_vbuffs=4) for every model loaded with this pipeline setup
//        - if the source model doesn't have joints AND weights. then the buffer #4 slot for the model will be NULL
//        - if the source model does have normal but does not have tangents, then buffer #2 will be created and tangents will be undefined
//      When rendering, you can customize which set of buffers you'll need based on the shader's input layout
//        - shadow map shader: can only fetch the buffer #1 (position)
//      The catch is when you setup your pipeline, all shaders should comply for one or more vertex-buffer formats
//      So in our example, every shader must take one of the 4 buffer formats or multiple of them
// vertex_attrs is all vertex attributes of the source model and is not related for vertex buffer formats
// gpu_t struct is created only for models without STREAM buffer flag
typedef struct rizz_model_mesh {
    int num_submeshes;
    int num_vertices;
    int num_indices;
    int num_vertex_attrs;
    int num_vbuffs;
    sg_index_type index_type;
    rizz_model_vertex_attribute* vertex_attrs;
    rizz_model_submesh* submeshes;

    struct cpu_t {
        void** vbuffs;      // arbitary struct for each vbuff (count=num_vbuffs)
        void*  ibuff;       // data type is based on index_type
    } cpu;

    struct gpu_t {
        sg_buffer* vbuffs;
        sg_buffer  ibuff;
    } gpu;
} rizz_model_mesh;

typedef struct rizz_model_node {
    char name[32];
    int mesh_id;        // =-1 if it's not renderable
    int parent_id;      // index to rizz_model.nodes
    int num_childs;     
    sx_tx3d local_tx;
    sx_aabb bounds;
    int* children;      // indices to rizz_model.nodes
} rizz_model_node;

// rizz_model will be stored inside rizz_asset handle 
typedef struct rizz_model {
    int num_meshes;
    int num_nodes;

    sx_tx3d root_tx;

    rizz_model_node* nodes;
    rizz_model_mesh* meshes;
} rizz_model;

typedef struct { uint32_t id; } rizz_model_instance;

static bool load_model(const char* filepath)
{
    sx_mem_block* mem = the_vfs->read(filepath, 0, the_core->alloc(RIZZ_MEMID_GAME));
    if (!mem) {
        return false;
    }

    cgltf_options options = {0};
    cgltf_data* data = NULL;
    cgltf_result result = cgltf_parse(&options, mem->data, mem->size, &data);
    sx_mem_destroy_block(mem);
    if (result != cgltf_result_success) {
        return false;
    }


    
    cgltf_free(data);
    return true;
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
    g_draw3d.stage = the_gfx->stage_register("main", (rizz_gfx_stage){ .id = 0 });
    sx_assert(g_draw3d.stage.id);

    // camera
    // projection: setup for perspective
    // view: Z-UP Y-Forward (like blender)
    sx_vec2 screen_size = the_app->sizef();
    const float view_width = screen_size.x;
    const float view_height = screen_size.y;
    the_camera->fps_init(&g_draw3d.cam, 50.0f, sx_rectwh(0, 0, view_width, view_height), 0.1f, 500.0f);
    the_camera->fps_lookat(&g_draw3d.cam, sx_vec3f(0, -3.0f, 3.0f), SX_VEC3_ZERO, SX_VEC3_UNITZ);
    return true;
}

static void shutdown(void)
{
}

static void update(float dt)
{
    const float move_speed = 3.0f;

    // update keyboard movement: WASD first-person
    if (the_app->key_pressed(RIZZ_APP_KEYCODE_A) || the_app->key_pressed(RIZZ_APP_KEYCODE_LEFT)) {
        the_camera->fps_strafe(&g_draw3d.cam, -move_speed*dt);
    }

    if (the_app->key_pressed(RIZZ_APP_KEYCODE_D) || the_app->key_pressed(RIZZ_APP_KEYCODE_RIGHT)) {
        the_camera->fps_strafe(&g_draw3d.cam, move_speed*dt);
    }

    if (the_app->key_pressed(RIZZ_APP_KEYCODE_W) || the_app->key_pressed(RIZZ_APP_KEYCODE_UP)) {
        the_camera->fps_forward(&g_draw3d.cam, move_speed*dt);
    }

    if (the_app->key_pressed(RIZZ_APP_KEYCODE_S) || the_app->key_pressed(RIZZ_APP_KEYCODE_DOWN)) {
        the_camera->fps_forward(&g_draw3d.cam, -move_speed*dt);
    }
}

static void render(void) 
{
    sg_pass_action pass_action = { .colors[0] = { SG_ACTION_CLEAR, {0.25f, 0.5f, 0.75f, 1.0f} },
                                   .depth = { SG_ACTION_CLEAR, 1.0f } };
    the_gfx->staged.begin(g_draw3d.stage);
    the_gfx->staged.begin_default_pass(&pass_action, the_app->width(), the_app->height());

    sx_mat4 proj = the_camera->perspective_mat(&g_draw3d.cam.cam);
    sx_mat4 view = the_camera->view_mat(&g_draw3d.cam.cam);
    sx_mat4 viewproj = sx_mat4_mul(&proj, &view);
    static sx_box boxes[100];
    static sx_color tints[100];
    static bool init_boxes = false;
    if (!init_boxes) {
        for (int i = 0; i < 100; i++) {
            boxes[i] = sx_box_set(sx_tx3d_setf(
                the_core->rand_range(-50.0f, 50.0f), the_core->rand_range(-50.0f, 50.0f), 0, 
                0, 0, sx_torad(the_core->rand_range(0, 90.0f))), 
                sx_vec3f(the_core->rand_range(1.0f, 2.0f), the_core->rand_range(1.0f, 2.0), 1.0f)); 
            tints[i] = sx_color4u(the_core->rand_range(0, 255), the_core->rand_range(0, 255), the_core->rand_range(0, 255), 255);
        }
        init_boxes = true;
    }
    
    the_prims->grid_xyplane_cam(1.0f, 5.0f, 50.0f, &g_draw3d.cam.cam, &viewproj);
    the_prims->draw_boxes(boxes, 100, &viewproj, RIZZ_PRIMS3D_MAPTYPE_CHECKER, tints);

    static sx_mat4 mat;
    static bool mat_init = false;
    if (!mat_init) {
        mat = sx_mat4_ident();
        mat_init = true;
    }

    sx_aabb aabb = sx_aabbwhd(1.0f, 2.0f, 0.5f);
    sx_vec3 pos = sx_vec3fv(mat.col4.f);
    aabb = sx_aabb_setpos(&aabb, pos);
    the_prims->draw_aabb(&aabb, &viewproj, SX_COLOR_WHITE);

    the_gfx->staged.end_pass();
    the_gfx->staged.end();


    the_imguix->gizmo_translate(&mat, &view, &proj, GIZMO_MODE_WORLD, NULL, NULL);
}

rizz_plugin_decl_main(draw3d, plugin, e)
{
    switch (e) {
    case RIZZ_PLUGIN_EVENT_STEP:
        update((float)sx_tm_sec(the_core->delta_tick()));
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
        the_prims = (rizz_api_prims3d*)plugin->api->get_api_byname("prims3d", 0);

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

rizz_plugin_decl_event_handler(nbody, e)
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

            if (!the_imguix->gizmo_using()) {
                the_camera->fps_pitch(&g_draw3d.cam, dy);
                the_camera->fps_yaw(&g_draw3d.cam, dx);
            }
        }
        break;
    default:
        break;
    }
}

rizz_game_decl_config(conf)
{
    conf->app_name = "";
    conf->app_version = 1000;
    conf->app_title = "draw3d";
    conf->app_flags |= RIZZ_APP_FLAG_HIGHDPI;
    conf->window_width = 800;
    conf->window_height = 600;
    conf->multisample_count = 4;
    conf->swap_interval = 1;
    conf->plugins[0] = "imgui";
    conf->plugins[1] = "3dtools";
}