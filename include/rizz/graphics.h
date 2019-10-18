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

// default name for API var, if you have a different name, set this macro before including this file
#ifndef RIZZ_GRAPHICS_API_VARNAME
#    define RIZZ_GRAPHICS_API_VARNAME the_gfx
#endif

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

typedef struct rizz_shader_refl_buffer {
    char name[32];
    int size_bytes;
    int binding;
    int array_stride;
} rizz_shader_refl_buffer;

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
    rizz_shader_refl_texture* storage_images;
    int num_storage_images;
    rizz_shader_refl_buffer* storage_buffers;
    int num_storage_buffers;
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
    sg_pixel_format fmt;    // request image format. only valid for basis files
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
//       Immediate mode is more of a direct and immediate graphics API
//       It is recommended to use staged API mentioned below,
//
// Staged API:   staged (deferred calls), multi-threaded API.
//               Contains only a selection of drawing functions
//               Can be called within worker threads spawned by `job_dispatch`, but with some rules
//               and restrictions
//
// Usage: always call begin_stage first, execute commands, then call end_stage
//        At the end of the frame step, all commands buffers will be merged and executed by the
//        rendering stages. Also, stages must be registered and setup before using staged
//        functions. (see below)
//
// Rule #1: in a worker threads, always end_stage before spawning and waiting for
//          another job because the command-buffer may change on thread-switch and drawing will be
//          messed up
//          Example of misuse:
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
//
// Rule #2: Do not destroy graphics objects (buffers/shaders/textures) during rendering work
//          This is actually like the multi-threaded asset usage pattern (see asset.h)
//          You should only destroy graphics objects when they are not being rendered or used
//
// Rule #3: The commands will be submitted to GPU at the end of the frame update automatically
//          But, you can use `presend_commands` and `commit_commands` to submit commands early
//          and prevent the GPU driver from doing too much work at the end. see below:
// 
// Common multi-threaded usage pattern is like as follows:
//  
// [thread #1]   ->             |---draw---|                       |---draw---|
// [thread #2]   ->             |---draw---|                       |---draw---|
// [thread #3]   ->             |---draw---|                       |---draw---|
// [game update] -> ----dispatch+---draw---|wait+present---dispatch+commit----|wait---draw--- <- end
//                                                  |                    |                         |
//  present called when no drawing is being done <-/                     |                         |
//       commit called during drawing (main thread) but after present <-/                          |
//    when frame is done, the framework will automatically execute and flush remaining commands <-/
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
    bool (*GLES_family)();
    void (*reset_state_cache)(void);

    // multi-threading
    // swaps the command buffers, makes previously submitted commands visible to `commit_commands`
    // NOTE: care must be taken when calling this function, first of all, it should be called on 
    //       the main thread. And should never be called when rendering jobs are running or 
    //       undefined behviour will occur. see documentation for more info.
    void (*present_commands)();
    // call this function to submit queued commands to the gpu. it will be also called automatically 
    // at the end of the frame. User must call `present_commands` before making this call
    // NOTE: should call this function only on the main thread. see documentation for more info.
    void (*commit_commands)();

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
    sg_buffer_desc (*query_buffer_defaults)(const sg_buffer_desc* desc);
    sg_image_desc (*query_image_defaults)(const sg_image_desc* desc);
    sg_shader_desc (*query_shader_defaults)(const sg_shader_desc* desc);
    sg_pipeline_desc (*query_pipeline_defaults)(const sg_pipeline_desc* desc);
    sg_pass_desc (*query_pass_defaults)(const sg_pass_desc* desc);

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
    sg_pipeline_desc* (*shader_bindto_pipeline)(const rizz_shader* shd, sg_pipeline_desc* pip_desc,
                                                const rizz_vertex_layout* vl);
    sg_pipeline_desc* (*shader_bindto_pipeline_sg)(sg_shader shd,
                                                   const rizz_shader_refl_input* inputs,
                                                   int num_inputs, sg_pipeline_desc* pip_desc,
                                                   const rizz_vertex_layout* vl);
    const rizz_shader* (*shader_get)(rizz_asset shader_asset);

    // texture
    sg_image (*texture_white)();
    sg_image (*texture_black)();
    sg_image (*texture_checker)();
    rizz_texture (*texture_create_checker)(int checker_size, int size, const sx_color colors[2]);
    const rizz_texture* (*texture_get)(rizz_asset texture_asset);

    // debug
    void (*debug_grid_xzplane)(float spacing, float spacing_bold, const sx_mat4* vp,
                               const sx_vec3 frustum[8]);
    void (*debug_grid_xyplane)(float spacing, float spacing_bold, const sx_mat4* vp,
                               const sx_vec3 frustum[8]);

    // info
    const rizz_gfx_trace_info* (*trace_info)();
} rizz_api_gfx;

#ifdef RIZZ_INTERNAL_API
bool rizz__gfx_init(const sx_alloc* alloc, const sg_desc* desc, bool enable_profile);
void rizz__gfx_release();
void rizz__gfx_trace_reset_frame_stats();
void rizz__gfx_execute_command_buffers_final();
void rizz__gfx_update();
void rizz__gfx_commit_gpu();
RIZZ_API rizz_api_gfx the__gfx;
#endif
