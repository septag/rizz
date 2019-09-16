//
// Copyright 2019 Sepehr Taghdisian (septag@github). All rights reserved.
// License: https://github.com/septag/rizz#license-bsd-2-clause
//
#pragma once

#include "_sg-types.h"

#include "sx/math.h"

// clang-format off
#define _rizz_concat_path_3(s1, s2, s3) sx_stringize(s1/s2/s3)
#define _rizz_shader_path_lang(_basepath, _lang, _filename) \
    _rizz_concat_path_3(_basepath, _lang, _filename)

// HELP: this macro can be used as a helper in including cross-platform shaders from source
//       appends _api_ (hlsl/glsl/gles/msl) between `_basepath` and `_filename`
//       only use this in including header shaders. example:
//       #include rizz_shader_path(shaders/include, myshader.h)
#define rizz_shader_path(_basepath, _filename) \
    _rizz_shader_path_lang(_basepath, RIZZ_GRAPHICS_SHADER_LANG, _filename)
// clang-format on

typedef struct sx_alloc sx_alloc;

typedef enum rizz_gfx_backend {
    RIZZ_GFX_BACKEND_GLCORE33,
    RIZZ_GFX_BACKEND_GLES2,
    RIZZ_GFX_BACKEND_GLES3,
    RIZZ_GFX_BACKEND_D3D11,
    RIZZ_GFX_BACKEND_METAL_IOS,
    RIZZ_GFX_BACKEND_METAL_MACOS,
    RIZZ_GFX_BACKEND_METAL_SIMULATOR,
    RIZZ_GFX_BACKEND_DUMMY,
} rizz_gfx_backend;

typedef enum rizz_shader_lang {
    RIZZ_SHADER_LANG_GLES,
    RIZZ_SHADER_LANG_HLSL,
    RIZZ_SHADER_LANG_MSL,
    RIZZ_SHADER_LANG_GLSL,
    _RIZZ_SHADER_LANG_COUNT
} rizz_shader_lang;

typedef enum rizz_shader_stage {
    RIZZ_SHADER_STAGE_VS,
    RIZZ_SHADER_STAGE_FS,
    RIZZ_SHADER_STAGE_CS,
    _RIZZ_SHADER_STAGE_COUNT
} rizz_shader_stage;

typedef enum rizz_shader_code_type {
    RIZZ_SHADER_CODE_SOURCE,
    RIZZ_SHADER_CODE_BYTECODE
} rizz_shader_code_type;

typedef struct rizz_shader_refl_input {
    char name[32];
    char semantic[32];
    int semantic_index;
    sg_vertex_format type;
} rizz_shader_refl_input;

typedef struct rizz_shader_refl_uniform_buffer {
    char name[32];
    int size_bytes;
    int binding;
    int array_size;    // for flattened ubos, we must provide array_size to the api with the type
                       // FLOAT4
} rizz_shader_refl_uniform_buffer;

typedef struct rizz_shader_refl_texture {
    char name[32];
    int binding;
    sg_image_type type;
} rizz_shader_refl_texture;

typedef struct rizz_shader_refl {
    rizz_shader_lang lang;
    rizz_shader_stage stage;
    int profile_version;
    char source_file[32];
    rizz_shader_refl_input* inputs;
    int num_inputs;
    rizz_shader_refl_texture* textures;
    int num_textures;
    rizz_shader_refl_uniform_buffer* uniform_buffers;
    int num_uniform_buffers;
    rizz_shader_code_type code_type;
    bool flatten_ubos;
} rizz_shader_refl;

// shader metadata
typedef struct rizz_shader_info {
    rizz_shader_refl_input inputs[SG_MAX_VERTEX_ATTRIBUTES];
    int num_inputs;
} rizz_shader_info;

typedef struct rizz_vertex_attr {
    const char* semantic;
    int semantic_idx;
    int offset;
    sg_vertex_format format;
    int buffer_index;
} rizz_vertex_attr;

typedef struct rizz_vertex_layout {
    rizz_vertex_attr attrs[SG_MAX_VERTEX_ATTRIBUTES];
} rizz_vertex_layout;

typedef struct rizz_shader {
    sg_shader shd;
    rizz_shader_info info;
} rizz_shader;

typedef struct rizz_texture_load_params {
    int first_mip;
    sg_filter min_filter;
    sg_filter mag_filter;
    sg_wrap wrap_u;
    sg_wrap wrap_v;
    sg_wrap wrap_w;
} rizz_texture_load_params;

// texture metadata
typedef struct rizz_texture_info {
    sg_image_type type;
    sg_pixel_format format;
    int mem_size_bytes;
    int width;
    int height;
    union {
        int depth;
        int layers;
    };
    int mips;
    int bpp;
} rizz_texture_info;

typedef struct rizz_texture {
    sg_image img;
    rizz_texture_info info;
} rizz_texture;

// text/font
enum rizz_text_flags_ {
    RIZZ_TEXT_ALIGN_CENTER = 0x01,
    RIZZ_TEXT_ALIGN_RIGHT = 0x02,
    RIZZ_TEXT_ALIGN_LEFT = 0x04,
    RIZZ_TEXT_RTL = 0x08,
    RIZZ_TEXT_MULTILINE = 0x10
};
typedef uint32_t rizz_text_flags;

enum rizz_font_flags_ {
    RIZZ_FONT_UNICODE = 0x1,
    RIZZ_FONT_ITALIC = 0x2,
    RIZZ_FONT_BOLD = 0x4,
    RIZZ_FONT_FIXED_HEIGHT = 0x8
};
typedef uint32_t rizz_font_flags;

typedef struct rizz_font {
    char name[32];
    int size;
    int line_height;
    int base;
    int img_width;
    int img_height;
    int char_width;
    rizz_font_flags flags;
    int16_t padding[4];
    int16_t spacing[2];
    rizz_asset img;
    int num_glyphs;
    int num_kerns;
} rizz_font;

typedef struct rizz_gfx_trace_info {
    int num_draws;
    int num_instances;
    int num_elements;
    int num_pipelines;
    int num_shaders;
    int num_passes;
    int num_images;
    int num_buffers;

    int num_apply_pipelines;
    int num_apply_passes;

    int64_t texture_size;
    int64_t texture_peak;

    int64_t buffer_size;
    int64_t buffer_peak;

    int64_t render_target_size;
    int64_t render_target_peak;
} rizz_gfx_trace_info;

typedef struct sjson_context sjson_context;    // shader_parse_reflection

// There are two kinds of drawing APIs:
//
// Immediate API: access directly to GPU graphics API. this is actually a thin wrapper over backend
//                Calls are executed immediately and sequentially, unlike _staged_ API (see below)
// Note: this API is NOT multi-threaded
//       Immediate mode is more of a lower-level graphics API
//       It is recommended to use staged API mentioned below,
//       because most of graphics primitives like sprites and text uses retained-mode API for it's
//       muti-threading capabilities
//
// Staged API:   staged (defrred calls), multi-threaded API.
//               Contains only a selection of async drawing functions
//               Can be called within worker threads spawned by `job_dispatch`, but with some rules
//               and restrictions
//
// Usage: always call begin_stage first, execute commands, then call end_stage
//        At the end of the frame step, all commands buffers will be merged and executed by the
//        rendering stages graph Stages must be registered and setup before using deferred
//        functions. (see below)
//
// Rule #1: in a worker threads, always end_stage before spawning and waiting for
//          another job because the command-buffer may change on thread-switch and drawing will be
//          messed up
//          Example:
//              { // dispatched job (worker-thread)
//                  job_dispatch(..);
//                  begin_stage(..)
//                  begin_pass(..);
//                  apply_viewport(..);
//                  job_wait_and_del(..)    // DON'T DO THIS: after wait, cmd-buffer will be changed
//                                          // instead, wait after end_stage call
//                  draw(..)
//                  end_stage(..)
//              }
// Rule #2: if you are submitting commands in other worker threads (sx_job_t dispatch jobs),
//          make sure you wait for them to complete before frame ends.
//          because there is no synchronization between main-thread command merge and worker threads
//          So the rule is for rendering jobs, you must dispatch and wait within a single frame
//          Example:
//              {   // dispatched job (worker-thread)
//                  begin_stage(..)
//                  begin_pass(..);
//                  draw(..)
//                  end_stage(..)
//              }
//
//              {   // main thread (game update)
//                  update();
//                  render();   // dispatches rendering jobs
//                  job_wait_and_del(rendering_jobs);
//              }
//
// Rule #3: Do not destroy graphics objects (buffers/shaders/textures) during rendering work
//          This is actually like the multi-threaded asset usage pattern (see asset.h)
//          You should only destroy graphics objects when they are not being rendered or used
//
// Here's multi-threaded command-queue work flow for clarification:
//      engine:frame {
//          execute_command_buffers();  // render data from previous frame
//          update_input();
//          update_plugins();
//          update_game() {
//              update_my_stuff();
//              spawn_some_render_jobs();
//              do_whatever();
//          }
//      }
//
// As you can see in the pseudo-code above, execute_command_buffers() happens at the start of the
// frame with the render-data submitted from the previous frame.
// So it's your responsibility to not destroy graphics resources where they are used in the
// current frame
// 
// Note: Difference between immediate and staged API is that staged API queues the draw commands
//       and submits them to the GPU at the begining of the _next_ frame. So the rendering will have
//       one frame latency, but the work load will be more balanced between CPU and GPU.
//       But immediate mode API sends commands directly to the GPU to and will be synced at the 
//       end of the current frame. So latency is lower, but it does not have multi-threading
//       capabilities.
//       The trade-off is that in theory, staged API will perform better in high detail scenes where 
//       there is lots of draws, especially if you use multi-threaded rendering. But in immediate 
//       API, the rendering latency is lower but would waste cycles waiting for GPU thus perform 
//       worse in high complexity scenes.
//
typedef struct rizz_api_gfx_draw {
    bool (*begin)(rizz_gfx_stage stage);
    void (*end)();

    void (*begin_default_pass)(const sg_pass_action* pass_action, int width, int height);
    void (*begin_pass)(sg_pass pass, const sg_pass_action* pass_action);
    void (*apply_viewport)(int x, int y, int width, int height, bool origin_top_left);
    void (*apply_scissor_rect)(int x, int y, int width, int height, bool origin_top_left);
    void (*apply_pipeline)(sg_pipeline pip);
    void (*apply_bindings)(const sg_bindings* bind);
    void (*apply_uniforms)(sg_shader_stage stage, int ub_index, const void* data, int num_bytes);
    void (*draw)(int base_element, int num_elements, int num_instances);
    void (*dispatch)(int thread_group_x, int thread_group_y, int thread_group_z);
    void (*end_pass)();

    void (*update_buffer)(sg_buffer buf, const void* data_ptr, int data_size);
    int (*append_buffer)(sg_buffer buf, const void* data_ptr, int data_size);
    void (*update_image)(sg_image img, const sg_image_content* data);

    // profile
    void (*begin_profile_sample)(const char* name, uint32_t* hash_cache);
    void (*end_profile_sample)();
} rizz_api_gfx_draw;

typedef struct rizz_api_gfx {
    rizz_api_gfx_draw imm;      // immediate draw API
    rizz_api_gfx_draw staged;   // staged (deferred calls) draw API

    rizz_gfx_backend (*backend)(void);
    bool (*GL_family)();
    void (*reset_state_cache)(void);

    // resource creation, destruction and updating
    sg_buffer (*make_buffer)(const sg_buffer_desc* desc);
    sg_image (*make_image)(const sg_image_desc* desc);
    sg_shader (*make_shader)(const sg_shader_desc* desc);
    sg_pipeline (*make_pipeline)(const sg_pipeline_desc* desc);
    sg_pass (*make_pass)(const sg_pass_desc* desc);

    // destroys (destroys are deferred calls, they execute after 1 frame, if object is not used)
    void (*destroy_buffer)(sg_buffer buf);
    void (*destroy_image)(sg_image img);
    void (*destroy_shader)(sg_shader shd);
    void (*destroy_pipeline)(sg_pipeline pip);
    void (*destroy_pass)(sg_pass pass);

    // get resource state (initial, alloc, valid, failed)
    bool (*query_buffer_overflow)(sg_buffer buf);
    sg_resource_state (*query_buffer_state)(sg_buffer buf);
    sg_resource_state (*query_image_state)(sg_image img);
    sg_resource_state (*query_shader_state)(sg_shader shd);
    sg_resource_state (*query_pipeline_state)(sg_pipeline pip);
    sg_resource_state (*query_pass_state)(sg_pass pass);

    // separate resource allocation and initialization (for async setup)
    sg_buffer (*alloc_buffer)(void);
    sg_image (*alloc_image)(void);
    sg_shader (*alloc_shader)(void);
    sg_pipeline (*alloc_pipeline)(void);
    sg_pass (*alloc_pass)(void);

    // init/fail objects
    void (*init_buffer)(sg_buffer buf_id, const sg_buffer_desc* desc);
    void (*init_image)(sg_image img_id, const sg_image_desc* desc);
    void (*init_shader)(sg_shader shd_id, const sg_shader_desc* desc);
    void (*init_pipeline)(sg_pipeline pip_id, const sg_pipeline_desc* desc);
    void (*init_pass)(sg_pass pass_id, const sg_pass_desc* desc);
    void (*fail_buffer)(sg_buffer buf_id);
    void (*fail_image)(sg_image img_id);
    void (*fail_shader)(sg_shader shd_id);
    void (*fail_pipeline)(sg_pipeline pip_id);
    void (*fail_pass)(sg_pass pass_id);

    // rendering contexts for multi-window rendering (optional)
    sg_context (*setup_context)(void);
    void (*activate_context)(sg_context ctx_id);
    void (*discard_context)(sg_context ctx_id);

    sg_trace_hooks (*install_trace_hooks)(const sg_trace_hooks* trace_hooks);
    sg_desc (*query_desc)(void);
    sg_buffer_info (*query_buffer_info)(sg_buffer buf);
    sg_image_info (*query_image_info)(sg_image img);
    sg_shader_info (*query_shader_info)(sg_shader shd);
    sg_pipeline_info (*query_pipeline_info)(sg_pipeline pip);
    sg_pass_info (*query_pass_info)(sg_pass pass);
    sg_features (*query_features)(void);
    sg_limits (*query_limits)(void);
    sg_pixelformat_info (*query_pixelformat)(sg_pixel_format fmt);

    // internal use (imgui plugin)
    void (*internal_state)(void** make_cmdbuff, int* make_cmdbuff_sz);

    // Stage:
    //     To performed deferred drawing calls, you should setup rendering stages on application
    //     init: `register_stage` Stages can be any group of drawing calls. For example, drawing a
    //     shadow-map for a light can be a stage Stages can also depend on each other. Multiple
    //     stages can depend on one stage to be finished, like a tree graph When the parent stage is
    //     disabled, all child stages are disabled
    rizz_gfx_stage (*stage_register)(const char* name, rizz_gfx_stage parent_stage);
    void (*stage_enable)(rizz_gfx_stage stage);
    void (*stage_disable)(rizz_gfx_stage stage);
    bool (*stage_isenabled)(rizz_gfx_stage stage);
    rizz_gfx_stage (*stage_find)(const char* name);

    // Shader
    rizz_shader_refl* (*shader_parse_reflection)(const sx_alloc* alloc, const char* stage_refl_json,
                                                 sjson_context* jctx);
    void (*shader_free_reflection)(rizz_shader_refl* refl, const sx_alloc* alloc);
    sg_shader_desc* (*shader_setup_desc)(sg_shader_desc* desc, const rizz_shader_refl* vs_refl,
                                         const void* vs, int vs_size,
                                         const rizz_shader_refl* fs_refl, const void* fs,
                                         int fs_size);
    rizz_shader (*shader_make_with_data)(const sx_alloc* alloc, uint32_t vs_data_size,
                                         const uint32_t* vs_data, uint32_t vs_refl_size,
                                         const uint32_t* vs_refl_json, uint32_t fs_data_size,
                                         const uint32_t* fs_data, uint32_t fs_refl_size,
                                         const uint32_t* fs_refl_json);
    sg_pipeline_desc* (*shader_bindto_pipeline)(rizz_shader* shd, sg_pipeline_desc* pip_desc,
                                                const rizz_vertex_layout* vl);
    sg_pipeline_desc* (*shader_bindto_pipeline_sg)(sg_shader shd,
                                                   const rizz_shader_refl_input* inputs,
                                                   int num_inputs, sg_pipeline_desc* pip_desc,
                                                   const rizz_vertex_layout* vl);
    // texture
    sg_image (*texture_white)();
    sg_image (*texture_black)();
    sg_image (*texture_checker)();
    rizz_texture (*texture_create_checker)(int checker_size, int size, const sx_color colors[2]);

    // debug
    void (*debug_grid_xzplane)(float spacing, float spacing_bold, const sx_mat4* vp,
                               const sx_vec3 frustum[8]);
    void (*debug_grid_xyplane)(float spacing, float spacing_bold, const sx_mat4* vp,
                               const sx_vec3 frustum[8]);

    // info
    const rizz_gfx_trace_info* (*trace_info)();
} rizz_api_gfx;

#ifdef RIZZ_INTERNAL_API
typedef struct rizz__gfx_cmdbuffer rizz__gfx_cmdbuffer;
bool rizz__gfx_init(const sx_alloc* alloc, const sg_desc* desc, bool enable_profile);
void rizz__gfx_release();
void rizz__gfx_trace_reset_frame_stats();
rizz__gfx_cmdbuffer* rizz__gfx_create_command_buffer(const sx_alloc* alloc);
void rizz__gfx_destroy_command_buffer(rizz__gfx_cmdbuffer* cb);
void rizz__gfx_execute_command_buffers();
void rizz__gfx_update();
void rizz__gfx_commit();

RIZZ_API rizz_api_gfx the__gfx;
#endif
