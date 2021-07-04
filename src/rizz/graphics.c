//
// Copyright 2019 Sepehr Taghdisian (septag@github). All rights reserved.
// License: https://github.com/septag/rizz#license-bsd-2-clause
//
// Glossary:
// rizz__texture_xxx: texture management functions (loading, reloading, etc..)
// rizz__shader_xxx: shader management functions (load, reload, reflection, ...)
// rizz__cb_gpu_command: command-buffer main commands
// rizz__cb_run_gpu_command: command-buffer deferred commands (this is actually where the command is executed) 
// rizz__gpu_command: overrides for immediate mode commands _sg_xxxx: sokol overrides
//
#include <alloca.h>

#include "basisut.h"

#include "rizz/config.h"

#include "sx/allocator.h"
#include "sx/array.h"
#include "sx/threads.h"
#include "sx/hash.h"
#include "sx/io.h"
#include "sx/lin-alloc.h"
#include "sx/os.h"
#include "sx/string.h"

#include "cj5/cj5.h"

#include "Remotery.h"

#if SX_COMPILER_CLANG  
#include <assert.h>  // static_assert ?! 
#endif

// clang-format off
#define MAX_STAGES                  1024
#define MAX_DEPTH                   64
#define STAGE_ORDER_DEPTH_BITS      6        
#define STAGE_ORDER_DEPTH_MASK      0xfc00    
#define STAGE_ORDER_ID_BITS         10       
#define STAGE_ORDER_ID_MASK         0x03ff   
#define CHECKER_TEXTURE_SIZE        128

static const sx_alloc*      g_gfx_alloc = NULL;

// Choose api based on the platform
#if RIZZ_GRAPHICS_API_D3D==11
#   define SOKOL_D3D11
#   define rmt__begin_gpu_sample(_name, _hash)  \
    RMT_OPTIONAL(RMT_USE_D3D11, (g_gfx.enable_profile ? _rmt_BeginD3D11Sample(_name, _hash) : 0))
#   define rmt__end_gpu_sample()                \
    RMT_OPTIONAL(RMT_USE_D3D11, (g_gfx.enable_profile ? _rmt_EndD3D11Sample() : 0))
    SX_PRAGMA_DIAGNOSTIC_PUSH()
    SX_PRAGMA_DIAGNOSTIC_IGNORED_MSVC(5105)
#   include <d3d11_2.h>
    SX_PRAGMA_DIAGNOSTIC_POP()
#elif RIZZ_GRAPHICS_API_METAL==1
#   define SOKOL_METAL
// disable profiling on metal, because it has some limitations. For example we can't micro-profile commands
// And raises some problems with the remotery
#   define rmt__begin_gpu_sample(_name, _hash) 
#   define rmt__end_gpu_sample()              
#elif RIZZ_GRAPHICS_API_GLES==21
#   define SOKOL_GLES2
#   define GL_GLEXT_PROTOTYPES
#   include <GLES2/gl2.h>
#   include <GLES2/gl2ext.h>
#   define rmt__begin_gpu_sample(_name, _hash) 
#   define rmt__end_gpu_sample()
#elif RIZZ_GRAPHICS_API_GLES==30
#   include <GLES3/gl3.h>
#   include <GLES3/gl3ext.h>
#   define SOKOL_GLES3
#   define rmt__begin_gpu_sample(_name, _hash) 
#   define rmt__end_gpu_sample()
#elif RIZZ_GRAPHICS_API_GL==33
#   include "flextGL/flextGL.h"
#   define SOKOL_GLCORE33
#   define rmt__begin_gpu_sample(_name, _hash)  \
    RMT_OPTIONAL(RMT_USE_OPENGL, (g_gfx.enable_profile ? _rmt_BeginOpenGLSample(_name, _hash) : 0))
#   define rmt__end_gpu_sample()                \
    RMT_OPTIONAL(RMT_USE_OPENGL, (g_gfx.enable_profile ? _rmt_EndOpenGLSample() : 0))
#else
#   error "Platform graphics is not supported"
#endif

// this is just a redirection in order to skip including "rizz.h"
static void rizz__gfx_log_error(const char* source_file, int line, const char* str);
static void rizz__gfx_assert_last_error(void);
static void rizz__gfx_set_shader_error(void);

#define SOKOL_MALLOC(s)             sx_malloc(g_gfx_alloc, s)
#define SOKOL_FREE(p)               sx_free(g_gfx_alloc, p)
#define SOKOL_LOG(s)                rizz__gfx_set_shader_error(); rizz__gfx_log_error(__FILE__, __LINE__, s)  
#define SOKOL_ASSERT(c)             if (!(c)) rizz__gfx_assert_last_error()

SX_PRAGMA_DIAGNOSTIC_PUSH()
SX_PRAGMA_DIAGNOSTIC_IGNORED_CLANG_GCC("-Wunused-function")
SX_PRAGMA_DIAGNOSTIC_IGNORED_CLANG_GCC("-Wunused-variable")
SX_PRAGMA_DIAGNOSTIC_IGNORED_MSVC(5105)
#define SOKOL_IMPL
#define SOKOL_API_DECL static
#define SOKOL_API_IMPL static
#define SOKOL_TRACE_HOOKS
#define SOKOL_NO_DEPRECATED
#include "sokol/sokol_gfx.h"
SX_PRAGMA_DIAGNOSTIC_POP();

#define SG_TYPES_ALREADY_DEFINED
#include "internal.h"
#undef SG_TYPES_ALREADY_DEFINED

// dds-ktx
#define DDSKTX_IMPLEMENT
#define DDSKTX_API static
#define ddsktx_memcpy(_dst, _src, _size)    sx_memcpy((_dst), (_src), (_size))
#define ddsktx_memset(_dst, _v, _size)      sx_memset((_dst), (_v), (_size))
#define ddsktx_assert(_a)                   sx_assert((_a))
#define ddsktx_strcpy(_dst, _src)           sx_strcpy((_dst), sizeof(_dst), (_src))
SX_PRAGMA_DIAGNOSTIC_PUSH()
SX_PRAGMA_DIAGNOSTIC_IGNORED_CLANG_GCC("-Wunused-function")
#include "dds-ktx/dds-ktx.h"
SX_PRAGMA_DIAGNOSTIC_POP()

// stb_image
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_STATIC
#define STBI_MALLOC(sz)                     sx_malloc(g_gfx_alloc, sz)
#define STBI_REALLOC(p,newsz)               sx_realloc(g_gfx_alloc, p, newsz)
#define STBI_FREE(p)                        sx_free(g_gfx_alloc, p)
SX_PRAGMA_DIAGNOSTIC_PUSH()
SX_PRAGMA_DIAGNOSTIC_IGNORED_CLANG_GCC("-Wshadow")
SX_PRAGMA_DIAGNOSTIC_IGNORED_CLANG_GCC("-Wunused-function")
SX_PRAGMA_DIAGNOSTIC_IGNORED_CLANG_GCC("-Wtype-limits")
SX_PRAGMA_DIAGNOSTIC_IGNORED_GCC("-Wmaybe-uninitialized")
#include "stb/stb_image.h"
SX_PRAGMA_DIAGNOSTIC_POP()

#ifdef _DEBUG
#    define rizz__queue_destroy(_a, _id, _alloc)                        \
        for (int __i = 0, __c = sx_array_count(_a); __i < __c; __i++) { \
            sx_assert(_a[__i].id != _id.id);                            \
        }                                                               \
        sx_array_push(_alloc, _a, _id)
#else
#   define rizz__queue_destroy(_a, _id, _alloc) \
    sx_array_push(_alloc, _a, _id)
#endif

// clang-format on

typedef struct rizz__sgs_chunk {
    int64_t pos;
    uint32_t size;
    uint32_t fourcc;
    int parent_id;
} rizz__sgs_chunk;

typedef struct rizz__gfx_texture_mgr {
    rizz_texture white_tex;
    rizz_texture black_tex;
    rizz_texture checker_tex;
    sg_filter    default_min_filter;
    sg_filter    default_mag_filter;
    int          default_aniso;
    int          default_first_mip;
} rizz__gfx_texture_mgr;

typedef enum rizz__gfx_command {
    GFX_COMMAND_BEGIN_DEFAULT_PASS = 0,
    GFX_COMMAND_BEGIN_PASS,
    GFX_COMMAND_APPLY_VIEWPORT,
    GFX_COMMAND_APPLY_SCISSOR_RECT,
    GFX_COMMAND_APPLY_PIPELINE,
    GFX_COMMAND_APPLY_BINDINGS,
    GFX_COMMAND_APPLY_UNIFORMS,
    GFX_COMMAND_DRAW,
    GFX_COMMAND_DISPATCH,
    GFX_COMMAND_END_PASS,
    GFX_COMMAND_UPDATE_BUFFER,
    GFX_COMMAND_UPDATE_IMAGE,
    GFX_COMMAND_APPEND_BUFFER,
    GFX_COMMAND_BEGIN_PROFILE,
    GFX_COMMAND_END_PROFILE,
    GFX_COMMAND_STAGE_PUSH,
    GFX_COMMAND_STAGE_POP,
    _GFX_COMMAND_COUNT,
    _GFX_COMMAND_ = INT32_MAX
} rizz__gfx_command;

typedef enum rizz__gfx_command_make {
    GFX_COMMAND_MAKE_BUFFER = 0,
    GFX_COMMAND_MAKE_IMAGE,
    GFX_COMMAND_MAKE_SHADER,
    GFX_COMMAND_MAKE_PIPELINE,
    GFX_COMMAND_MAKE_PASS,
    _GFX_COMMAND_MAKE_COUNT,
    _GFX_COMMAND_MAKE_ = INT32_MAX
} rizz__gfx_command_make;

typedef enum rizz__gfx_stage_state {
    STAGE_STATE_NONE = 0,
    STAGE_STATE_SUBMITTING,
    STAGE_STATE_DONE,
    _STAGE_STATE_ = INT32_MAX
} rizz__gfx_stage_state;

typedef struct rizz__gfx_cmdbuffer_ref {
    uint32_t key;    // sort key. higher bits: rizz__gfx_stage.order, lower bits: cmd_idx
    int cmdbuffer_idx;
    rizz__gfx_command cmd;
    int params_offset;
} rizz__gfx_cmdbuffer_ref;

typedef struct rizz__gfx_cmdbuffer {
    const sx_alloc* alloc;
    uint8_t* params_buff;             // sx_array
    rizz__gfx_cmdbuffer_ref* refs;    // sx_array
    rizz_gfx_stage running_stage;
    int index;
    uint16_t stage_order;
    uint16_t cmd_idx;
} rizz__gfx_cmdbuffer;

// stream-buffers are used to emulate sg_append_buffer behaviour
typedef struct rizz__gfx_stream_buffer {
    sg_buffer buf;
    sx_atomic_int offset;
    int size;
} rizz__gfx_stream_buffer;

typedef struct rizz__gfx_stage {
    char name[32];
    uint32_t name_hash;
    rizz__gfx_stage_state state;
    rizz_gfx_stage parent;
    rizz_gfx_stage child;
    rizz_gfx_stage next;
    rizz_gfx_stage prev;
    uint16_t order;    // dependency order (higher bits: depth, lower bits: stage_id)
    bool enabled;
    bool single_enabled;
} rizz__gfx_stage;

#ifdef SOKOL_METAL
typedef struct rizz__pip_mtl {
    sg_pipeline pip;
    sg_pipeline_desc desc;
} rizz__pip_mtl;
#endif

typedef struct rizz__trace_gfx {
    rizz_gfx_trace_info t;
    sx_mem_writer make_cmds_writer;
    sg_trace_hooks hooks;
    rizz_gfx_perframe_trace_info* active_trace;
} rizz__trace_gfx;

typedef struct rizz__gfx_source_loc {
    const char* file;
    uint32_t line;
#if SX_ARCH_64BIT
    uint32_t _reserved;
#endif
} rizz__gfx_source_loc;

typedef struct rizz__gfx {
    rizz__gfx_stage* stages;                    // sx_array
    rizz__gfx_cmdbuffer* cmd_buffers_feed;      // commands that are queued (sx_array)
    rizz__gfx_cmdbuffer* cmd_buffers_render;    // commands that are being rendered (sx_array)
    sx_lock_t stage_lk;
    rizz__gfx_texture_mgr tex_mgr;
    #ifdef SOKOL_METAL
        rizz__pip_mtl* pips;    // sx_array: keep track of pipelines for shader hot-reloads
    #else
        sg_pipeline* pips;
    #endif
    #ifdef SOKOL_D3D11
        ID3D11DeviceContext2* d3d11_ctx;
        bool                  d3d11_has_marker;
    #endif
    rizz__gfx_stream_buffer* stream_buffs;    // sx_array: streaming buffers for append_buffers

    sg_buffer* destroy_buffers;
    sg_shader* destroy_shaders;
    sg_pipeline* destroy_pips;
    sg_pass* destroy_passes;
    sg_image* destroy_images;

    char cur_stage_name[32];

    rizz__trace_gfx trace;
    rizz__gfx_source_loc cur_source_loc;

    bool enable_profile;
    bool record_make_commands;
    bool last_shader_error;
} rizz__gfx;


typedef uint8_t* (*rizz__run_command_cb)(uint8_t* buff);

#define SORT_NAME rizz__gfx
#define SORT_TYPE rizz__gfx_cmdbuffer_ref
#define SORT_CMP(x, y) ((x).key < (y).key ? -1 : 1)
SX_PRAGMA_DIAGNOSTIC_PUSH()
SX_PRAGMA_DIAGNOSTIC_IGNORED_MSVC(4267)
SX_PRAGMA_DIAGNOSTIC_IGNORED_MSVC(4244)
SX_PRAGMA_DIAGNOSTIC_IGNORED_MSVC(4146)
SX_PRAGMA_DIAGNOSTIC_IGNORED_MSVC(4505)
SX_PRAGMA_DIAGNOSTIC_IGNORED_CLANG_GCC("-Wunused-function")
SX_PRAGMA_DIAGNOSTIC_IGNORED_CLANG("-Wshorten-64-to-32")
#include "sort/sort.h"
SX_PRAGMA_DIAGNOSTIC_POP()

static rizz__gfx g_gfx;

static void rizz__gfx_set_shader_error(void)
{
    g_gfx.last_shader_error = true;
}

static void rizz__gfx_assert_last_error(void)
{
    const char* stage;
    if (g_gfx.cur_stage_name[0]) {
        stage = g_gfx.cur_stage_name;
    } else {
        stage = "[unset]";
    }

    if (g_gfx.cur_source_loc.file) {
        the__core.print_error(0, g_gfx.cur_source_loc.file, g_gfx.cur_source_loc.line, "Stage = '%s'", stage);
    } else {
        rizz__log_error("Stage = '%s'", stage);
    }

    sx_assert_always(0);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// @sokol_gfx
#if defined(SOKOL_D3D11)
_SOKOL_PRIVATE bool _sapp_win32_utf8_to_wide(const char* src, wchar_t* dst, int dst_num_bytes)
{
    SOKOL_ASSERT(src && dst && (dst_num_bytes > 1));
    memset(dst, 0, dst_num_bytes);
    const int dst_chars = dst_num_bytes / sizeof(wchar_t);
    const int dst_needed = MultiByteToWideChar(CP_UTF8, 0, src, -1, 0, 0);
    if ((dst_needed > 0) && (dst_needed < dst_chars)) {
        MultiByteToWideChar(CP_UTF8, 0, src, -1, dst, dst_chars);
        return true;
    } else {
        /* input string doesn't fit into destination buffer */
        return false;
    }
}

_SOKOL_PRIVATE void _sg_set_pipeline_shader(_sg_pipeline_t* pip, sg_shader shader_id,
                                            _sg_shader_t* shd, const rizz_shader_info* info,
                                            const sg_pipeline_desc* desc)
{
    SOKOL_ASSERT(shd->slot.state == SG_RESOURCESTATE_VALID);
    SOKOL_ASSERT(shd->d3d11.cs || (shd->d3d11.vs_blob && shd->d3d11.vs_blob_length > 0));
    sx_unused(desc);
    sx_unused(info);

    pip->shader = shd;
    pip->cmn.shader_id = shader_id;
}
#elif defined(SOKOL_METAL)
_SOKOL_PRIVATE void _sg_set_pipeline_shader(_sg_pipeline_t* pip, sg_shader shader_id,
                                            _sg_shader_t* shd, const rizz_shader_info* info,
                                            const sg_pipeline_desc* desc)
{
    sx_unused(info);
    SOKOL_ASSERT(shd->slot.state == SG_RESOURCESTATE_VALID);

    pip->shader = shd;
    pip->cmn.shader_id = shader_id;
    sg_pipeline_desc desc_def = _sg_pipeline_desc_defaults(desc);
    desc = &desc_def;

    // TODO: recreate pipeline descriptor
    SOKOL_ASSERT(pip);
    _sg_mtl_release_resource(_sg.mtl.frame_index, pip->mtl.rps);

    /* create vertex-descriptor */
    MTLVertexDescriptor* vtx_desc = [MTLVertexDescriptor vertexDescriptor];
    for (int attr_index = 0; attr_index < SG_MAX_VERTEX_ATTRIBUTES; attr_index++) {
        const sg_vertex_attr_desc* a_desc = &desc->layout.attrs[attr_index];
        if (a_desc->format == SG_VERTEXFORMAT_INVALID) {
            break;
        }
        SOKOL_ASSERT((a_desc->buffer_index >= 0) && (a_desc->buffer_index < SG_MAX_SHADERSTAGE_BUFFERS));
        vtx_desc.attributes[attr_index].format = _sg_mtl_vertex_format(a_desc->format);
        vtx_desc.attributes[attr_index].offset = a_desc->offset;
        vtx_desc.attributes[attr_index].bufferIndex = a_desc->buffer_index + SG_MAX_SHADERSTAGE_UBS;
        pip->cmn.vertex_layout_valid[a_desc->buffer_index] = true;
    }
    for (int layout_index = 0; layout_index < SG_MAX_SHADERSTAGE_BUFFERS; layout_index++) {
        if (pip->cmn.vertex_layout_valid[layout_index]) {
            const sg_buffer_layout_desc* l_desc = &desc->layout.buffers[layout_index];
            const int mtl_vb_slot = layout_index + SG_MAX_SHADERSTAGE_UBS;
            SOKOL_ASSERT(l_desc->stride > 0);
            vtx_desc.layouts[mtl_vb_slot].stride = l_desc->stride;
            vtx_desc.layouts[mtl_vb_slot].stepFunction = _sg_mtl_step_function(l_desc->step_func);
            vtx_desc.layouts[mtl_vb_slot].stepRate = l_desc->step_rate;
        }
    }

    /* render-pipeline descriptor */
    MTLRenderPipelineDescriptor* rp_desc = [[MTLRenderPipelineDescriptor alloc] init];
    rp_desc.vertexDescriptor = vtx_desc;
    SOKOL_ASSERT(shd->mtl.stage[SG_SHADERSTAGE_VS].mtl_func != _SG_MTL_INVALID_SLOT_INDEX);
    rp_desc.vertexFunction = _sg_mtl_id(shd->mtl.stage[SG_SHADERSTAGE_VS].mtl_func);
    SOKOL_ASSERT(shd->mtl.stage[SG_SHADERSTAGE_FS].mtl_func != _SG_MTL_INVALID_SLOT_INDEX);
    rp_desc.fragmentFunction = _sg_mtl_id(shd->mtl.stage[SG_SHADERSTAGE_FS].mtl_func);
    rp_desc.sampleCount = desc->rasterizer.sample_count;
    rp_desc.alphaToCoverageEnabled = desc->rasterizer.alpha_to_coverage_enabled;
    rp_desc.alphaToOneEnabled = NO;
    rp_desc.rasterizationEnabled = YES;
    rp_desc.depthAttachmentPixelFormat = _sg_mtl_pixel_format(desc->blend.depth_format);
    if (desc->blend.depth_format == SG_PIXELFORMAT_DEPTH_STENCIL) {
        rp_desc.stencilAttachmentPixelFormat = _sg_mtl_pixel_format(desc->blend.depth_format);
    }

    const int att_count = desc->blend.color_attachment_count;
    for (int i = 0; i < att_count; i++) {
        rp_desc.colorAttachments[i].pixelFormat = _sg_mtl_pixel_format(desc->blend.color_formats[i]);
        rp_desc.colorAttachments[i].writeMask = _sg_mtl_color_write_mask((sg_color_mask)desc->blend.color_write_mask);
        rp_desc.colorAttachments[i].blendingEnabled = desc->blend.enabled;
        rp_desc.colorAttachments[i].alphaBlendOperation = _sg_mtl_blend_op(desc->blend.op_alpha);
        rp_desc.colorAttachments[i].rgbBlendOperation = _sg_mtl_blend_op(desc->blend.op_rgb);
        rp_desc.colorAttachments[i].destinationAlphaBlendFactor = _sg_mtl_blend_factor(desc->blend.dst_factor_alpha);
        rp_desc.colorAttachments[i].destinationRGBBlendFactor = _sg_mtl_blend_factor(desc->blend.dst_factor_rgb);
        rp_desc.colorAttachments[i].sourceAlphaBlendFactor = _sg_mtl_blend_factor(desc->blend.src_factor_alpha);
        rp_desc.colorAttachments[i].sourceRGBBlendFactor = _sg_mtl_blend_factor(desc->blend.src_factor_rgb);
    }
    NSError* err = NULL;
    id<MTLRenderPipelineState> mtl_rps = [_sg.mtl.device newRenderPipelineStateWithDescriptor:rp_desc error:&err];
    _SG_OBJC_RELEASE(rp_desc);
    if (nil == mtl_rps) {
        SOKOL_ASSERT(err);
        SOKOL_LOG([err.localizedDescription UTF8String]);
        return;
    }

    pip->mtl.rps = _sg_mtl_add_resource(mtl_rps);
}
#elif defined(SOKOL_GLCORE33) || defined(SOKOL_GLES2) || defined(SOKOL_GLES3)
_SOKOL_PRIVATE void _sg_set_pipeline_shader(_sg_pipeline_t* pip, sg_shader shader_id,
                                            _sg_shader_t* shd, const rizz_shader_info* info,
                                            const sg_pipeline_desc* desc)
{
    SOKOL_ASSERT(shd->slot.state == SG_RESOURCESTATE_VALID);
    sx_unused(desc);

    pip->shader = shd;
    pip->cmn.shader_id = shader_id;

    // check that vertex attributes are not changed
    // When vertex attributes change, the required data to re-evaluate attributes will be missing
    // from the program like vertex buffer stride and offsets This scenario should not happen.
    // because the program (cpu side) has to change at the same time, which is not possible
    int num_attrs = info->num_inputs;
    for (int attr_index = 0; attr_index < num_attrs; attr_index++) {
        const rizz_shader_refl_input* in = &info->inputs[attr_index];
        SOKOL_ASSERT(in->name);
        GLint attr_loc = glGetAttribLocation(shd->gl.prog, in->name);
        if (attr_loc != -1) {
            _sg_gl_attr_t* gl_attr = &pip->gl.attrs[attr_loc];
            SOKOL_ASSERT(gl_attr->size == (uint8_t)_sg_gl_vertexformat_size(in->type));
            SOKOL_ASSERT(gl_attr->type == _sg_gl_vertexformat_type(in->type));
            SOKOL_ASSERT(gl_attr->normalized == _sg_gl_vertexformat_normalized(in->type));
            sx_unused(gl_attr);
        }
    }
}
#endif

static void sg_set_pipeline_shader(sg_pipeline pip_id, sg_shader prev_shader_id,
                                   sg_shader shader_id, const rizz_shader_info* info,
                                   const sg_pipeline_desc* desc)
{
    SOKOL_ASSERT(pip_id.id != SG_INVALID_ID);
    _sg_pipeline_t* pip = _sg_lookup_pipeline(&_sg.pools, pip_id.id);
    SOKOL_ASSERT(pip && pip->slot.state == SG_RESOURCESTATE_VALID);
    if (pip->cmn.shader_id.id == prev_shader_id.id) {
        _sg_shader_t* shd = _sg_lookup_shader(&_sg.pools, shader_id.id);
        SOKOL_ASSERT(shd && shd->slot.state == SG_RESOURCESTATE_VALID);
        _sg_set_pipeline_shader(pip, shader_id, shd, info, desc);
    }
}

static void sg_map_buffer(sg_buffer buf_id, int offset, const void* data, int num_bytes)
{
    _sg_buffer_t* buf = _sg_lookup_buffer(&_sg.pools, buf_id.id);
    if (buf) {
        /* rewind append cursor in a new frame */
        if (buf->cmn.map_frame_index != _sg.frame_index) {
            buf->cmn.append_pos = 0;
            buf->cmn.append_overflow = false;
        }

        if ((offset + num_bytes) > buf->cmn.size) {
            buf->cmn.append_overflow = true;
        }

        if (buf->slot.state == SG_RESOURCESTATE_VALID) {
            buf->cmn.append_pos = offset;    // alter append_pos, so we write at offset
            if (_sg_validate_append_buffer(buf, data, num_bytes)) {
                if (!buf->cmn.append_overflow && (num_bytes > 0)) {
                    /* update and append and map on same buffer in same frame not allowed */
                    SOKOL_ASSERT(buf->cmn.update_frame_index != _sg.frame_index);
                    SOKOL_ASSERT(buf->cmn.append_frame_index != _sg.frame_index);
                    _sg_append_buffer(buf, data, num_bytes,
                                      buf->cmn.map_frame_index != _sg.frame_index);
                    buf->cmn.map_frame_index = _sg.frame_index;
                }
            }
        }
    } else {
        sx_assertf(0, "invalid buf_id");
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// @texture
static inline sg_image_type rizz__texture_get_type(const ddsktx_texture_info* tc)
{
    sx_assertf(!((tc->flags & DDSKTX_TEXTURE_FLAG_CUBEMAP) && (tc->num_layers > 1)),
              "cube-array textures are not supported");
    sx_assertf(!(tc->num_layers > 1 && tc->depth > 1), "3d-array textures are not supported");

    if (tc->flags & DDSKTX_TEXTURE_FLAG_CUBEMAP)
        return SG_IMAGETYPE_CUBE;
    else if (tc->num_layers > 1)
        return SG_IMAGETYPE_ARRAY;
    else if (tc->depth > 1)
        return SG_IMAGETYPE_3D;
    else
        return SG_IMAGETYPE_2D;
}

static inline sg_pixel_format rizz__texture_get_texture_format(ddsktx_format fmt)
{
    // clang-format off
    switch (fmt) {
    case DDSKTX_FORMAT_BGRA8:   return SG_PIXELFORMAT_RGBA8;    // TODO: FIXME ? 
    case DDSKTX_FORMAT_RGBA8:   return SG_PIXELFORMAT_RGBA8;
    case DDSKTX_FORMAT_RGBA16F: return SG_PIXELFORMAT_RGBA16F;
    case DDSKTX_FORMAT_R32F:    return SG_PIXELFORMAT_R32F;
    case DDSKTX_FORMAT_R16F:    return SG_PIXELFORMAT_R16F;
    case DDSKTX_FORMAT_BC1:     return SG_PIXELFORMAT_BC1_RGBA;
    case DDSKTX_FORMAT_BC2:     return SG_PIXELFORMAT_BC2_RGBA;
    case DDSKTX_FORMAT_BC3:     return SG_PIXELFORMAT_BC3_RGBA;
    case DDSKTX_FORMAT_BC4:     return SG_PIXELFORMAT_BC4_R;
    case DDSKTX_FORMAT_BC5:     return SG_PIXELFORMAT_BC5_RG;
    case DDSKTX_FORMAT_BC6H:    return SG_PIXELFORMAT_BC6H_RGBF;
    case DDSKTX_FORMAT_BC7:     return SG_PIXELFORMAT_BC7_RGBA;
    case DDSKTX_FORMAT_PTC12:   return SG_PIXELFORMAT_PVRTC_RGB_2BPP;
    case DDSKTX_FORMAT_PTC14:   return SG_PIXELFORMAT_PVRTC_RGB_4BPP;
    case DDSKTX_FORMAT_PTC12A:  return SG_PIXELFORMAT_PVRTC_RGBA_2BPP;
    case DDSKTX_FORMAT_PTC14A:  return SG_PIXELFORMAT_PVRTC_RGBA_4BPP;
    case DDSKTX_FORMAT_ETC2:    return SG_PIXELFORMAT_ETC2_RGB8;
    case DDSKTX_FORMAT_ETC2A:   return SG_PIXELFORMAT_ETC2_RGB8A1;
    default:                    return SG_PIXELFORMAT_NONE;
    }
    // clang-format on
}

typedef struct basisut_transcode_data {
    basisut_transcoder_texture_format fmt;
    int mip_size[SG_MAX_MIPMAPS];
} basisut_transcode_data;

static rizz_asset_load_data rizz__texture_on_prepare(const rizz_asset_load_params* params,
                                                     const sx_mem_block* mem)
{
    const sx_alloc* alloc = params->alloc ? params->alloc : g_gfx_alloc;

    rizz_texture* tex = sx_calloc(alloc, sizeof(rizz_texture));
    if (!tex) {
        sx_out_of_memory();
        return (rizz_asset_load_data){ .obj = { 0 } };
    }

    rizz_texture_info* info = &tex->info;
    bool is_basis = false;
    char ext[32];
    sx_os_path_ext(ext, sizeof(ext), params->path);
    if (sx_strequalnocase(ext, ".basis")) {
        if (basisut_validate_header(mem->data, (uint32_t)mem->size)) {
            bool r = basisut_image_info(mem->data, (uint32_t)mem->size, info);
            is_basis = true;
            sx_unused(r);
            sx_assert(r);
        } else {
            rizz__log_warn("reading texture '%s' metadata failed", params->path);
            sx_free(alloc, tex);
            return (rizz_asset_load_data){ .obj = { 0 } };
        }
    } else if (sx_strequalnocase(ext, ".dds") || sx_strequalnocase(ext, ".ktx")) {
        ddsktx_texture_info tc = { 0 };
        ddsktx_error err;
        if (ddsktx_parse(&tc, mem->data, (uint32_t)mem->size, &err)) {
            info->type = rizz__texture_get_type(&tc);
            info->format = rizz__texture_get_texture_format(tc.format);
            if (info->type == SG_IMAGETYPE_ARRAY) {
                info->layers = tc.num_layers;
            } else if (info->type == SG_IMAGETYPE_3D) {
                info->depth = tc.depth;
            } else {
                info->layers = 1;
            }
            info->mem_size_bytes = tc.size_bytes;
            info->width = tc.width;
            info->height = tc.height;
            info->mips = tc.num_mips;
            info->bpp = tc.bpp;
        } else {
            rizz__log_warn("reading texture '%s' metadata failed: %s", params->path, err.msg);
            sx_memset(info, 0x0, sizeof(rizz_texture_info));
        }
    } else {
        // try to use stbi to load the image
        int comp;
        if (stbi_info_from_memory(mem->data, (int)mem->size, &info->width, &info->height, &comp)) {
            sx_assertf(!stbi_is_16_bit_from_memory(mem->data, (int)mem->size),
                      "images with 16bit color channel are not supported: %s", params->path);
            sx_assertf(info->width > 0 && info->height > 0, "invalid image size (%dx%d): %s", 
                       info->width, info->height, params->path);
            info->type = SG_IMAGETYPE_2D;
            info->format = SG_PIXELFORMAT_RGBA8;    // always convert to RGBA
            info->mem_size_bytes = 4 * info->width * info->height;
            info->layers = 1;
            info->mips = 1;
            info->bpp = 32;
        } else {
            rizz__log_warn("reading image '%s' metadata failed: %s", params->path, stbi_failure_reason());
            sx_memset(info, 0x0, sizeof(rizz_texture_info));
        }
    }

    tex->img = the__gfx.alloc_image();
    sx_assert(tex->img.id);

    void* user_data;
    // create extra buffer for basis transcoding
    if (is_basis) {
        const rizz_texture_load_params* tparams = params->params;
        sx_assertf(tparams->fmt != _SG_PIXELFORMAT_DEFAULT, "fmt must be defined for basis files");

        // clang-format off
        basisut_transcoder_texture_format basis_fmt;
        switch (tparams->fmt) {
        case SG_PIXELFORMAT_ETC2_RGB8:  basis_fmt = cTFETC1; break;
        case SG_PIXELFORMAT_ETC2_RGBA8: basis_fmt = cTFETC2; break;
        case SG_PIXELFORMAT_ETC2_RG11:  basis_fmt = cTFETC2_EAC_RG11; break;
        case SG_PIXELFORMAT_BC1_RGBA:   basis_fmt = cTFBC1; break;
        case SG_PIXELFORMAT_BC3_RGBA:   basis_fmt = cTFBC3; break;
        case SG_PIXELFORMAT_BC4_R:      basis_fmt = cTFBC4; break;
        case SG_PIXELFORMAT_BC5_RG:     basis_fmt = cTFBC5; break;
        case SG_PIXELFORMAT_BC7_RGBA:   basis_fmt = cTFBC7_M5;  break;
        case SG_PIXELFORMAT_PVRTC_RGBA_4BPP:    basis_fmt = cTFPVRTC1_4_RGBA;   break;
        case SG_PIXELFORMAT_PVRTC_RGB_4BPP:     basis_fmt = cTFPVRTC1_4_RGB;    break;
        case SG_PIXELFORMAT_RGBA8:              basis_fmt = cTFRGBA32;          break;
        default:
            rizz__log_warn(
                "parsing texture '%s' failed. transcoding of this format is not supported");
            sx_assert(0);
            return (rizz_asset_load_data){ .obj = { 0 } };
        }
        // clang-format on

        tex->info.format = tparams->fmt;
        int w = tex->info.width;
        int h = tex->info.height;
        int num_mips = tex->info.mips;
        int num_images = tex->info.layers;

        size_t total_sz = sizeof(sg_image_desc) + basisut_transcoder_bytesize() + sizeof(basisut_transcode_data);
        int mip_size[SG_MAX_MIPMAPS];

        // calculate the buffer sizes needed for holding all the output pixels
        sx_assert(num_mips < SG_MAX_MIPMAPS);

        for (int i = 0; i < num_images; i++) {
            for (int mip = 0; mip < num_mips; mip++) {
                if (mip >= tparams->first_mip) {
                    int image_sz = _sg_surface_pitch(tparams->fmt, w, h, 1);
                    mip_size[mip - tparams->first_mip] = image_sz;
                    total_sz += image_sz;
                }

                w >>= 1;
                h >>= 1;
                if (w == 0 || h == 0) {
                    break;
                }
            }
        }

        uint8_t* buff = (uint8_t*)sx_malloc(g_gfx_alloc, total_sz);
        if (!buff) {
            sx_out_of_memory();
            return (rizz_asset_load_data){ .obj = { 0 } };
        }
        user_data = buff;
        buff += sizeof(sg_image_desc) + basisut_transcoder_bytesize();
        basisut_transcode_data* transcode_data = (basisut_transcode_data*)buff;
        transcode_data->fmt = basis_fmt;
        sx_memcpy(transcode_data->mip_size, mip_size, sizeof(mip_size));
    } else {
        user_data = sx_malloc(g_gfx_alloc, sizeof(sg_image_desc));
    }

    return (rizz_asset_load_data){ .obj = { .ptr = tex }, .user1 = user_data };
}

static bool rizz__texture_on_load(rizz_asset_load_data* data, const rizz_asset_load_params* params,
                                  const sx_mem_block* mem)
{
    const rizz_texture_load_params* tparams = params->params;
    rizz_texture* tex = data->obj.ptr;
    sg_image_desc* desc = data->user1;

    int first_mip = tparams->first_mip ? tparams->first_mip : g_gfx.tex_mgr.default_first_mip;
    if (first_mip >= tex->info.mips) {
        first_mip = tex->info.mips - 1;
    }
    int num_mips = tex->info.mips - first_mip;

    {   // fix width/height/mips of the texture
        int w = tex->info.width;
        int h = tex->info.height;
        for (int i = 0; i < first_mip; i++) {
            w >>= 1;
            h >>= 1;
        }
        tex->info.mips = num_mips;
        tex->info.width = w;
        tex->info.height = h;
    }

    sx_assert(desc);
    *desc = (sg_image_desc) {
        .type = tex->info.type,
        .width = tex->info.width,
        .height = tex->info.height,
        .num_slices = tex->info.layers,
        .num_mipmaps = num_mips, 
        .pixel_format = tex->info.format,
        .min_filter = tparams->min_filter != 0 ? tparams->min_filter : g_gfx.tex_mgr.default_min_filter,
        .mag_filter = tparams->mag_filter != 0 ? tparams->mag_filter : g_gfx.tex_mgr.default_mag_filter,
        .wrap_u = tparams->wrap_u,
        .wrap_v = tparams->wrap_v,
        .wrap_w = tparams->wrap_w,
        .max_anisotropy = tparams->aniso ? (uint32_t)tparams->aniso : (uint32_t)g_gfx.tex_mgr.default_aniso,
        .srgb = tparams->srgb
    };

    // see if we have metadata, and parse it and override the texture desc
    for (uint32_t i = 0; i < params->num_meta; i++) {
        if (sx_strequal(params->metas[i].key, "wrap")) {
            if (sx_strequal(params->metas[i].value, "repeat"))
                desc->wrap_u = desc->wrap_v = desc->wrap_w = SG_WRAP_REPEAT;
            else if (sx_strequal(params->metas[i].value, "clamp_to_edge"))
                desc->wrap_u = desc->wrap_v = desc->wrap_w = SG_WRAP_CLAMP_TO_EDGE;
            else if (sx_strequal(params->metas[i].value, "clamp_to_border"))
                desc->wrap_u = desc->wrap_v = desc->wrap_w = SG_WRAP_CLAMP_TO_BORDER;
            else if (sx_strequal(params->metas[i].value, "mirrored_repeat"))
                desc->wrap_u = desc->wrap_v = desc->wrap_w = SG_WRAP_MIRRORED_REPEAT;
        }
        else if (sx_strequal(params->metas[i].key, "filter")) {
            if (sx_strequal(params->metas[i].value, "nearest"))
                desc->min_filter = desc->mag_filter = SG_FILTER_NEAREST;
            else if (sx_strequal(params->metas[i].value, "linear"))
                desc->min_filter = desc->mag_filter = SG_FILTER_LINEAR;
            else if (sx_strequal(params->metas[i].value, "nearest_mipmap_nearest"))
                desc->min_filter = desc->mag_filter = SG_FILTER_NEAREST_MIPMAP_NEAREST;
            else if (sx_strequal(params->metas[i].value, "nearest_mipmap_linear"))
                desc->min_filter = desc->mag_filter = SG_FILTER_NEAREST_MIPMAP_LINEAR;
            else if (sx_strequal(params->metas[i].value, "linear_mipmap_nearest"))
                desc->min_filter = desc->mag_filter = SG_FILTER_LINEAR_MIPMAP_NEAREST;
            else if (sx_strequal(params->metas[i].value, "linear_mipmap_linear"))
                desc->min_filter = desc->mag_filter = SG_FILTER_LINEAR_MIPMAP_LINEAR;
        }
        else if (sx_strequal(params->metas[i].key, "aniso")) {
            desc->max_anisotropy = sx_toint(params->metas[i].value);
        }
        else if (sx_strequal(params->metas[i].key, "srgb")) {
            desc->srgb = sx_tobool(params->metas[i].value);
        }
    }

    char ext[32];
    sx_os_path_ext(ext, sizeof(ext), params->path);

    if (sx_strequalnocase(ext, ".basis")) {
        sx_assert(tparams->fmt != _SG_PIXELFORMAT_DEFAULT);
        if (tparams->fmt != _SG_PIXELFORMAT_DEFAULT) {
            uint8_t* transcoder_obj_buffer = (uint8_t*)(desc + 1);
            void* trans = basisut_start_transcoding(transcoder_obj_buffer, mem->data, (uint32_t)mem->size);
            sx_assert(trans);

            // we have extra buffers for this particular type of file
            basisut_transcode_data* transcode_data =
                (basisut_transcode_data*)(transcoder_obj_buffer + basisut_transcoder_bytesize());
            uint8_t* transcode_buff = (uint8_t*)(transcode_data + 1);

            int num_images = tex->info.type == SG_IMAGETYPE_2D ? 1 : tex->info.layers;
            int bytes_per_block =
                basisut_format_is_uncompressed(transcode_data->fmt)
                    ? basisut_get_uncompressed_bytes_per_pixel(transcode_data->fmt)
                    : (int)basisut_get_bytes_per_block(transcode_data->fmt);

            for (int i = 0; i < num_images; i++) {
                for (int mip = first_mip; mip < num_mips; mip++) {
                    int dst_mip = mip - first_mip;
                    int mip_size = transcode_data->mip_size[dst_mip];
                    bool r = basisut_transcode_image_level(
                        trans, mem->data, (uint32_t)mem->size, 0, mip, transcode_buff,
                        mip_size / bytes_per_block, transcode_data->fmt, 0);
                    sx_unused(r);
                    sx_assertf(r, "basis transcode failed");
                    desc->content.subimage[i][dst_mip].ptr = transcode_buff;
                    desc->content.subimage[i][dst_mip].size = mip_size;
                    transcode_buff += mip_size;
                }
            }
        } else {
            rizz__log_warn("parsing texture '%s' failed", params->path);
            return false;
        }
    } else if (sx_strequalnocase(ext, ".dds") || sx_strequalnocase(ext, ".ktx")) {
        ddsktx_texture_info tc = { 0 };
        ddsktx_error err;
        if (ddsktx_parse(&tc, mem->data, (int)mem->size, &err)) {
            sx_assert(tc.num_mips <= SG_MAX_MIPMAPS);

            switch (tex->info.type) {
            case SG_IMAGETYPE_2D: {
                for (int mip = first_mip; mip < tc.num_mips; mip++) {
                    int dst_mip = mip - first_mip;
                    ddsktx_sub_data sub_data;
                    ddsktx_get_sub(&tc, &sub_data, mem->data, (int)mem->size, 0, 0, mip);
                    desc->content.subimage[0][dst_mip].ptr = sub_data.buff;
                    desc->content.subimage[0][dst_mip].size = sub_data.size_bytes;
                }
            } break;
            case SG_IMAGETYPE_CUBE: {
                for (int face = 0; face < DDSKTX_CUBE_FACE_COUNT; face++) {
                    for (int mip = first_mip; mip < tc.num_mips; mip++) {
                        int dst_mip = mip - first_mip;
                        ddsktx_sub_data sub_data;
                        ddsktx_get_sub(&tc, &sub_data, mem->data, (int)mem->size, 0, face, mip);
                        desc->content.subimage[face][dst_mip].ptr = sub_data.buff;
                        desc->content.subimage[face][dst_mip].size = sub_data.size_bytes;
                    }
                }
            } break;
            case SG_IMAGETYPE_3D: {
                for (int depth = 0; depth < tc.depth; depth++) {
                    for (int mip = first_mip; mip < tc.num_mips; mip++) {
                        int dst_mip = mip - first_mip;
                        ddsktx_sub_data sub_data;
                        ddsktx_get_sub(&tc, &sub_data, mem->data, (int)mem->size, 0, depth, mip);
                        desc->content.subimage[depth][dst_mip].ptr = sub_data.buff;
                        desc->content.subimage[depth][dst_mip].size = sub_data.size_bytes;
                    }
                }
            } break;
            case SG_IMAGETYPE_ARRAY: {
                for (int array = 0; array < tc.num_layers; array++) {
                    for (int mip = first_mip; mip < tc.num_mips; mip++) {
                        int dst_mip = mip - first_mip;
                        ddsktx_sub_data sub_data;
                        ddsktx_get_sub(&tc, &sub_data, mem->data, (int)mem->size, array, 0, mip);
                        desc->content.subimage[array][dst_mip].ptr = sub_data.buff;
                        desc->content.subimage[array][dst_mip].size = sub_data.size_bytes;
                    }
                }
            } break;
            default:
                break;
            }
        } else {
            rizz__log_warn("parsing texture '%s' failed: %s", params->path, err.msg);
            return false;
        }
    } else {
        int w, h, comp;
        stbi_uc* pixels = stbi_load_from_memory(mem->data, (int)mem->size, &w, &h, &comp, 4);
        if (pixels) {
            sx_assert(tex->info.width == w && tex->info.height == h);
            desc->content.subimage[0][0].ptr = pixels;
            desc->content.subimage[0][0].size = w * h * 4;
        } else {
            rizz__log_warn("parsing image '%s' failed: %s", params->path, stbi_failure_reason());
            return false;
        }
    }

    return true;
}

static void rizz__texture_on_finalize(rizz_asset_load_data* data,
                                      const rizz_asset_load_params* params, const sx_mem_block* mem)
{
    sx_unused(mem);

    rizz_texture* tex = data->obj.ptr;
    sg_image_desc* desc = data->user1;
    sx_assert(desc);

    char basename[64];
    desc->label = the__core.str_alloc(&tex->info.name_hdl, 
                                      sx_os_path_basename(basename, sizeof(basename), params->path));

    the__gfx.init_image(tex->img, desc);

    char ext[32];
    sx_os_path_ext(ext, sizeof(ext), params->path);
    // TODO: do something better in case of stbi
    if (!sx_strequalnocase(ext, ".dds") && !sx_strequalnocase(ext, ".ktx") && !sx_strequalnocase(ext, ".basis")) {
        sx_assert(desc->content.subimage[0][0].ptr);
        stbi_image_free((void*)desc->content.subimage[0][0].ptr);
    }

    sx_free(g_gfx_alloc, data->user1);
}

static void rizz__texture_on_reload(rizz_asset handle, rizz_asset_obj prev_obj,
                                    const sx_alloc* alloc)
{
    sx_unused(prev_obj);
    sx_unused(handle);
    sx_unused(alloc);
}

static void rizz__texture_on_release(rizz_asset_obj obj, const sx_alloc* alloc)
{
    rizz_texture* tex = obj.ptr;
    sx_assert(tex);

    if (!alloc)
        alloc = g_gfx_alloc;

    if (tex->img.id)
        the__gfx.destroy_image(tex->img);

    if (tex->info.name_hdl)
        the__core.str_free(tex->info.name_hdl);

    sx_free(alloc, tex);
}

static rizz_texture rizz__texture_create_checker(int checker_size, int size,
                                                 const sx_color colors[2])
{
    sx_assertf(size % 4 == 0, "size must be multiple of four");
    sx_assertf(size % checker_size == 0, "checker_size must be dividable by size");

    int size_bytes = size * size * sizeof(uint32_t);
    uint32_t* pixels = sx_malloc(g_gfx_alloc, size_bytes);

    // split into tiles and color them
    int tiles_x = size / checker_size;
    int tiles_y = size / checker_size;
    int num_tiles = tiles_x * tiles_y;
    rizz_texture tex;

    rizz__with_temp_alloc(tmp_alloc) {
        sx_ivec2* poss = sx_malloc(tmp_alloc, sizeof(sx_ivec2) * num_tiles);
        sx_assert(poss);
        int _x = 0, _y = 0;
        for (int i = 0; i < num_tiles; i++) {
            poss[i] = sx_ivec2i(_x, _y);
            _x += checker_size;
            if (_x >= size) {
                _x = 0;
                _y += checker_size;
            }
        }

        int color_idx = 0;
        for (int i = 0; i < num_tiles; i++) {
            sx_ivec2 p = poss[i];
            sx_color c = colors[color_idx];
            if (i == 0 || ((i + 1) % tiles_x) != 0)
                color_idx = !color_idx;
            int end_x = p.x + checker_size;
            int end_y = p.y + checker_size;
            for (int y = p.y; y < end_y; y++) {
                for (int x = p.x; x < end_x; x++) {
                    int pixel = x + y * size;
                    pixels[pixel] = c.n;
                }
            }
        }

        tex = (rizz_texture){   .img = the__gfx.make_image(&(sg_image_desc){
                                .width = size,
                                .height = size,
                                .num_mipmaps = 1,
                                .pixel_format = SG_PIXELFORMAT_RGBA8,
                                .content = (sg_image_content){ .subimage[0][0].ptr = pixels,
                                                               .subimage[0][0].size = size_bytes },
                                .label = "rizz_checker_texture" }),
                            .info = (rizz_texture_info){ .type = SG_IMAGETYPE_2D,
                                                         .format = SG_PIXELFORMAT_RGBA8,
                                                         .mem_size_bytes = size_bytes,
                                                         .width = size,
                                                         .height = size,
                                                         .layers = 1,
                                                         .mips = 1,
                                                         .bpp = 32 } };

        sx_free(tmp_alloc, poss);
        sx_free(g_gfx_alloc, pixels);
    }
    return tex;
}

static void rizz__texture_init()
{
    static uint32_t k_white_pixel = 0xffffffff;
    static uint32_t k_black_pixel = 0xff000000;
    g_gfx.tex_mgr.white_tex = (rizz_texture){
        .img = the__gfx.make_image(&(sg_image_desc){
            .width = 1,
            .height = 1,
            .num_mipmaps = 1,
            .pixel_format = SG_PIXELFORMAT_RGBA8,
            .content = (sg_image_content){ .subimage[0][0].ptr = &k_white_pixel,
                                           .subimage[0][0].size = sizeof(k_white_pixel) },
            .label = "rizz_white_texture_1x1"}),
        .info = (rizz_texture_info){ .type = SG_IMAGETYPE_2D,
                                     .format = SG_PIXELFORMAT_RGBA8,
                                     .mem_size_bytes = sizeof(k_white_pixel),
                                     .width = 1,
                                     .height = 1,
                                     .layers = 1,
                                     .mips = 1,
                                     .bpp = 32 }
    };

    g_gfx.tex_mgr.black_tex = (rizz_texture){
        .img = the__gfx.make_image(&(sg_image_desc){
            .width = 1,
            .height = 1,
            .num_mipmaps = 1,
            .pixel_format = SG_PIXELFORMAT_RGBA8,
            .content = (sg_image_content){ .subimage[0][0].ptr = &k_black_pixel,
                                           .subimage[0][0].size = sizeof(k_black_pixel) },
            .label = "rizz_black_texture_1x1" }),
        .info = (rizz_texture_info){ .type = SG_IMAGETYPE_2D,
                                     .format = SG_PIXELFORMAT_RGBA8,
                                     .mem_size_bytes = sizeof(k_black_pixel),
                                     .width = 1,
                                     .height = 1,
                                     .layers = 1,
                                     .mips = 1,
                                     .bpp = 32 }
    };

    const sx_color checker_colors[] = { sx_color4u(255, 0, 255, 255),
                                        sx_color4u(255, 255, 255, 255) };
    g_gfx.tex_mgr.checker_tex = rizz__texture_create_checker(CHECKER_TEXTURE_SIZE / 2,
                                                             CHECKER_TEXTURE_SIZE, checker_colors);

    the__asset.register_asset_type("texture",
                                   (rizz_asset_callbacks){ .on_prepare = rizz__texture_on_prepare,
                                                           .on_load = rizz__texture_on_load,
                                                           .on_finalize = rizz__texture_on_finalize,
                                                           .on_reload = rizz__texture_on_reload,
                                                           .on_release = rizz__texture_on_release },
                                   "rizz_texture_load_params", sizeof(rizz_texture_load_params),
                                   (rizz_asset_obj){ .ptr = &g_gfx.tex_mgr.checker_tex },
                                   (rizz_asset_obj){ .ptr = &g_gfx.tex_mgr.white_tex }, 0);

    // init basis
    basisut_init(g_gfx_alloc);
}

static void rizz__texture_release()
{
    if (g_gfx.tex_mgr.white_tex.img.id)
        the__gfx.destroy_image(g_gfx.tex_mgr.white_tex.img);
    if (g_gfx.tex_mgr.black_tex.img.id)
        the__gfx.destroy_image(g_gfx.tex_mgr.black_tex.img);
    if (g_gfx.tex_mgr.checker_tex.img.id)
        the__gfx.destroy_image(g_gfx.tex_mgr.checker_tex.img);
    basisut_release();
}

static sg_image rizz__texture_white()
{
    return g_gfx.tex_mgr.white_tex.img;
}

static sg_image rizz__texture_black()
{
    return g_gfx.tex_mgr.black_tex.img;
}

static sg_image rizz__texture_checker()
{
    return g_gfx.tex_mgr.checker_tex.img;
}

static const rizz_texture* rizz__texture_get(rizz_asset texture_asset)
{
#if RIZZ_DEV_BUILD
    sx_assert_always(sx_strequal(the__asset.type_name(texture_asset), "texture") && "asset handle is not a texture");
#endif
    return (const rizz_texture*)the__asset.obj(texture_asset).ptr;
}

static void rizz__texture_set_default_quality(sg_filter min_filter, sg_filter mag_filter, int aniso, int first_mip)
{
    g_gfx.tex_mgr.default_min_filter = min_filter;
    g_gfx.tex_mgr.default_mag_filter = mag_filter;
    g_gfx.tex_mgr.default_aniso = aniso;
    g_gfx.tex_mgr.default_first_mip = first_mip;
}

static void rizz__texture_default_quality(sg_filter* min_filter, sg_filter* mag_filter, int* aniso, int* first_mip)
{
    if (min_filter)
        *min_filter = g_gfx.tex_mgr.default_min_filter;
    if (mag_filter) 
        *mag_filter = g_gfx.tex_mgr.default_mag_filter;
    if (aniso)
        *aniso = g_gfx.tex_mgr.default_aniso;
    if (first_mip)
        *first_mip = g_gfx.tex_mgr.default_first_mip;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// @shader
// Begin: SGS format
#pragma pack(push, 1)

#define SGS_CHUNK sx_makefourcc('S', 'G', 'S', ' ')
#define SGS_CHUNK_STAG sx_makefourcc('S', 'T', 'A', 'G')
#define SGS_CHUNK_REFL sx_makefourcc('R', 'E', 'F', 'L')
#define SGS_CHUNK_CODE sx_makefourcc('C', 'O', 'D', 'E')
#define SGS_CHUNK_DATA sx_makefourcc('D', 'A', 'T', 'A')

#define SGS_LANG_GLES sx_makefourcc('G', 'L', 'E', 'S')
#define SGS_LANG_HLSL sx_makefourcc('H', 'L', 'S', 'L')
#define SGS_LANG_GLSL sx_makefourcc('G', 'L', 'S', 'L')
#define SGS_LANG_MSL sx_makefourcc('M', 'S', 'L', ' ')
#define SGS_LANG_GLES sx_makefourcc('G', 'L', 'E', 'S')

#define SGS_VERTEXFORMAT_FLOAT sx_makefourcc('F', 'L', 'T', '1')
#define SGS_VERTEXFORMAT_FLOAT2 sx_makefourcc('F', 'L', 'T', '2')
#define SGS_VERTEXFORMAT_FLOAT3 sx_makefourcc('F', 'L', 'T', '3')
#define SGS_VERTEXFORMAT_FLOAT4 sx_makefourcc('F', 'L', 'T', '4')
#define SGS_VERTEXFORMAT_INT sx_makefourcc('I', 'N', 'T', '1')
#define SGS_VERTEXFORMAT_INT2 sx_makefourcc('I', 'N', 'T', '2')
#define SGS_VERTEXFORMAT_INT3 sx_makefourcc('I', 'N', 'T', '3')
#define SGS_VERTEXFORMAT_INT4 sx_makefourcc('I', 'N', 'T', '4')

#define SGS_STAGE_VERTEX sx_makefourcc('V', 'E', 'R', 'T')
#define SGS_STAGE_FRAGMENT sx_makefourcc('F', 'R', 'A', 'G')
#define SGS_STAGE_COMPUTE sx_makefourcc('C', 'O', 'M', 'P')

#define SGS_IMAGEDIM_1D sx_makefourcc('1', 'D', ' ', ' ')
#define SGS_IMAGEDIM_2D sx_makefourcc('2', 'D', ' ', ' ')
#define SGS_IMAGEDIM_3D sx_makefourcc('3', 'D', ' ', ' ')
#define SGS_IMAGEDIM_CUBE sx_makefourcc('C', 'U', 'B', 'E')
#define SGS_IMAGEDIM_RECT sx_makefourcc('R', 'E', 'C', 'T')
#define SGS_IMAGEDIM_BUFFER sx_makefourcc('B', 'U', 'F', 'F')
#define SGS_IMAGEDIM_SUBPASS sx_makefourcc('S', 'U', 'B', 'P')

// SGS chunk
struct sgs_chunk {
    uint32_t lang;           // sgs_shader_lang
    uint32_t profile_ver;    //
};

// REFL
struct sgs_chunk_refl {
    char name[32];
    uint32_t num_inputs;
    uint32_t num_textures;
    uint32_t num_uniform_buffers;
    uint32_t num_storage_images;
    uint32_t num_storage_buffers;
    uint16_t flatten_ubos;
    uint16_t debug_info;

    // inputs: sgs_refl_input[num_inputs]
    // uniform-buffers: sgs_refl_uniformbuffer[num_uniform_buffers]
    // textures: sgs_refl_texture[num_textures]
};

// RFCS
struct sgs_chunk_cs_refl {
    uint32_t num_storages_images;
    uint32_t num_storage_buffers;

    // storage_images: sgs_refl_texture[num_storage_images]
    // storage_buffers: sgs_refl_buffer[num_storage_buffers]
};

struct sgs_refl_input {
    char name[32];
    int32_t loc;
    char semantic[32];
    uint32_t semantic_index;
    uint32_t format;
};

struct sgs_refl_texture {
    char name[32];
    int32_t binding;
    uint32_t image_dim;
    uint8_t multisample;
    uint8_t is_array;
};

struct sgs_refl_buffer {
    char name[32];
    int32_t binding;
    uint32_t size_bytes;
    uint32_t array_stride;
};

struct sgs_refl_uniformbuffer {
    char name[32];
    int32_t binding;
    uint32_t size_bytes;
    uint16_t array_size;
};

#pragma pack(pop)
// End: SGS format

static rizz_shader_lang rizz__shader_str_to_lang(const char* s)
{
    if (sx_strequal(s, "gles"))
        return RIZZ_SHADER_LANG_GLES;
    else if (sx_strequal(s, "hlsl"))
        return RIZZ_SHADER_LANG_HLSL;
    else if (sx_strequal(s, "msl"))
        return RIZZ_SHADER_LANG_MSL;
    else if (sx_strequal(s, "glsl"))
        return RIZZ_SHADER_LANG_GLSL;
    else
        return _RIZZ_SHADER_LANG_COUNT;
}

static rizz_shader_lang rizz__shader_fourcc_to_lang(uint32_t fourcc)
{
    if (fourcc == SGS_LANG_GLES)
        return RIZZ_SHADER_LANG_GLES;
    else if (fourcc == SGS_LANG_HLSL)
        return RIZZ_SHADER_LANG_HLSL;
    else if (fourcc == SGS_LANG_MSL)
        return RIZZ_SHADER_LANG_MSL;
    else if (fourcc == SGS_LANG_GLSL)
        return RIZZ_SHADER_LANG_GLSL;
    else
        return _RIZZ_SHADER_LANG_COUNT;
}

static sg_vertex_format rizz__shader_str_to_vertex_format(const char* s)
{
    if (sx_strequal(s, "float"))
        return SG_VERTEXFORMAT_FLOAT;
    else if (sx_strequal(s, "float2"))
        return SG_VERTEXFORMAT_FLOAT2;
    else if (sx_strequal(s, "float3"))
        return SG_VERTEXFORMAT_FLOAT3;
    else if (sx_strequal(s, "float4"))
        return SG_VERTEXFORMAT_FLOAT4;
    else if (sx_strequal(s, "byte4"))
        return SG_VERTEXFORMAT_BYTE4;
    else if (sx_strequal(s, "ubyte4"))
        return SG_VERTEXFORMAT_UBYTE4;
    else if (sx_strequal(s, "ubyte4n"))
        return SG_VERTEXFORMAT_UBYTE4N;
    else if (sx_strequal(s, "short2"))
        return SG_VERTEXFORMAT_SHORT2;
    else if (sx_strequal(s, "short2n"))
        return SG_VERTEXFORMAT_SHORT2N;
    else if (sx_strequal(s, "short4"))
        return SG_VERTEXFORMAT_SHORT4;
    else if (sx_strequal(s, "short4n"))
        return SG_VERTEXFORMAT_SHORT4N;
    else if (sx_strequal(s, "uint10n2"))
        return SG_VERTEXFORMAT_UINT10_N2;
    else
        return _SG_VERTEXFORMAT_NUM;
}

static sg_vertex_format rizz__shader_fourcc_to_vertex_format(uint32_t fourcc, const char* semantic)
{
    if (fourcc == SGS_VERTEXFORMAT_FLOAT)
        return SG_VERTEXFORMAT_FLOAT;
    else if (fourcc == SGS_VERTEXFORMAT_FLOAT2)
        return SG_VERTEXFORMAT_FLOAT2;
    else if (fourcc == SGS_VERTEXFORMAT_FLOAT3)
        return SG_VERTEXFORMAT_FLOAT3;
    else if (fourcc == SGS_VERTEXFORMAT_FLOAT4 && sx_strequal(semantic, "COLOR"))
        return SG_VERTEXFORMAT_FLOAT4;
    else if (fourcc == SGS_VERTEXFORMAT_FLOAT4)
        return SG_VERTEXFORMAT_FLOAT4;
    else
        return _SG_VERTEXFORMAT_NUM;
}

static sg_image_type rizz__shader_str_to_texture_type(const char* s, bool array)
{
    if (array && sx_strequal(s, "2d"))
        return SG_IMAGETYPE_ARRAY;
    else if (sx_strequal(s, "2d"))
        return SG_IMAGETYPE_2D;
    else if (sx_strequal(s, "3d"))
        return SG_IMAGETYPE_3D;
    else if (sx_strequal(s, "cube"))
        return SG_IMAGETYPE_CUBE;
    else
        return _SG_IMAGETYPE_DEFAULT;
}

static sg_image_type rizz__shader_fourcc_to_texture_type(uint32_t fourcc, bool array)
{
    if (array && fourcc == SGS_IMAGEDIM_2D) {
        return SG_IMAGETYPE_ARRAY;
    } else if (!array) {
        if (fourcc == SGS_IMAGEDIM_2D)
            return SG_IMAGETYPE_2D;
        else if (fourcc == SGS_IMAGEDIM_3D)
            return SG_IMAGETYPE_3D;
        else if (fourcc == SGS_IMAGEDIM_CUBE)
            return SG_IMAGETYPE_CUBE;
    }
    return _SG_IMAGETYPE_DEFAULT;
}

static rizz_shader_refl* rizz__shader_parse_reflect_bin(const sx_alloc* alloc,
                                                        const void* refl_data, uint32_t refl_size)
{
    sx_mem_reader r;
    sx_mem_init_reader(&r, refl_data, refl_size);

    struct sgs_chunk_refl refl_chunk;
    sx_mem_read_var(&r, refl_chunk);

    uint32_t total_sz = sizeof(rizz_shader_refl) +
                        sizeof(rizz_shader_refl_input) * refl_chunk.num_inputs +
                        sizeof(rizz_shader_refl_uniform_buffer) * refl_chunk.num_uniform_buffers +
                        sizeof(rizz_shader_refl_texture) * refl_chunk.num_textures +
                        sizeof(rizz_shader_refl_texture) * refl_chunk.num_storage_images +
                        sizeof(rizz_shader_refl_buffer) * refl_chunk.num_storage_buffers;

    rizz_shader_refl* refl = (rizz_shader_refl*)sx_malloc(alloc, total_sz);
    if (!refl) {
        sx_out_of_memory();
        return NULL;
    }
    uint8_t* buff = (uint8_t*)(refl + 1);

    sx_memset(refl, 0x0, sizeof(rizz_shader_refl));
    sx_strcpy(refl->source_file, sizeof(refl->source_file), refl_chunk.name);
    refl->flatten_ubos = refl_chunk.flatten_ubos ? true : false;
    refl->num_inputs = refl_chunk.num_inputs;
    refl->num_textures = refl_chunk.num_textures;
    refl->num_uniform_buffers = refl_chunk.num_uniform_buffers;
    refl->num_storage_images = refl_chunk.num_storage_images;
    refl->num_storage_buffers = refl_chunk.num_storage_buffers;

    if (refl_chunk.num_inputs) {
        refl->inputs = (rizz_shader_refl_input*)buff;
        buff += sizeof(rizz_shader_refl_input) * refl_chunk.num_inputs;

        for (uint32_t i = 0; i < refl_chunk.num_inputs; i++) {
            struct sgs_refl_input in;
            sx_mem_read_var(&r, in);
            refl->inputs[i] = (rizz_shader_refl_input){
                .semantic_index = in.semantic_index,
                .type = rizz__shader_fourcc_to_vertex_format(in.format, in.semantic)
            };
            sx_strcpy(refl->inputs[i].name, sizeof(refl->inputs[i].name), in.name);
            sx_strcpy(refl->inputs[i].semantic, sizeof(refl->inputs[i].semantic), in.semantic);
        }
    }

    if (refl_chunk.num_uniform_buffers) {
        refl->uniform_buffers = (rizz_shader_refl_uniform_buffer*)buff;
        buff += sizeof(rizz_shader_refl_uniform_buffer) * refl_chunk.num_uniform_buffers;

        for (uint32_t i = 0; i < refl_chunk.num_uniform_buffers; i++) {
            struct sgs_refl_uniformbuffer u;
            sx_mem_read_var(&r, u);
            refl->uniform_buffers[i] = (rizz_shader_refl_uniform_buffer){
                .size_bytes = u.size_bytes, 
                .binding = u.binding, 
                .array_size = u.array_size
            };
            sx_strcpy(refl->uniform_buffers[i].name, sizeof(refl->uniform_buffers[i].name), u.name);
        }
    }

    if (refl_chunk.num_textures) {
        refl->textures = (rizz_shader_refl_texture*)buff;
        buff += sizeof(rizz_shader_refl_texture) * refl_chunk.num_textures;

        for (uint32_t i = 0; i < refl_chunk.num_textures; i++) {
            struct sgs_refl_texture t;
            sx_mem_read_var(&r, t);
            refl->textures[i] = (rizz_shader_refl_texture){
                .binding = t.binding,
                .type = rizz__shader_fourcc_to_texture_type(t.image_dim, t.is_array ? true : false)
            };
            sx_strcpy(refl->textures[i].name, sizeof(refl->textures[i].name), t.name);
        }
    }

    if (refl_chunk.num_storage_images) {
        refl->storage_images = (rizz_shader_refl_texture*)buff;
        buff += sizeof(rizz_shader_refl_texture) * refl_chunk.num_storage_images;

        for (uint32_t i = 0; i < refl_chunk.num_storage_images; i++) {
            struct sgs_refl_texture img;
            sx_mem_read_var(&r, img);
            refl->storage_images[i] =
                (rizz_shader_refl_texture){ .binding = img.binding,
                                            .type = rizz__shader_fourcc_to_texture_type(
                                                img.image_dim, img.is_array ? true : false) };
            sx_strcpy(refl->storage_images[i].name, sizeof(refl->storage_images[i].name), img.name);
        }
    }

    if (refl_chunk.num_storage_buffers) {
        refl->storage_buffers = (rizz_shader_refl_buffer*)buff;
        buff += sizeof(rizz_shader_refl_buffer) * refl_chunk.num_storage_buffers;

        for (uint32_t i = 0; i < refl_chunk.num_storage_buffers; i++) {
            struct sgs_refl_buffer b;
            sx_mem_read_var(&r, b);
            refl->storage_buffers[i] = (rizz_shader_refl_buffer){ .size_bytes = b.size_bytes,
                                                                  .binding = b.binding,
                                                                  .array_stride = b.array_stride };
            sx_strcpy(refl->storage_buffers[i].name, sizeof(refl->storage_buffers[i].name), b.name);
        }
    }

    return refl;
}


static rizz_shader_refl* rizz__shader_parse_reflect_json(const sx_alloc* alloc,
                                                         const char* stage_refl_json,
                                                         int stage_refl_json_len)
{
    cj5_token tokens[1024];
    const int max_tokens = sizeof(tokens) / sizeof(cj5_token);
    cj5_result jres = cj5_parse(stage_refl_json, stage_refl_json_len, tokens, max_tokens);
    if (jres.error) {
        if (jres.error == CJ5_ERROR_OVERFLOW) {
            cj5_token* ntokens = (cj5_token*)sx_malloc(alloc, sizeof(cj5_token) * jres.num_tokens);
            if (!ntokens) {
                sx_out_of_memory();
                return NULL;
            }
            jres = cj5_parse(stage_refl_json, stage_refl_json_len - 1, ntokens, jres.num_tokens);
            if (jres.error) {
                rizz__log_error("loading shader reflection failed: invalid json");
                return NULL;
            }
        }

        rizz__log_error("loading shader reflection failed: invalid json, line: %d",
                        jres.error_line);
    }

    // count everything and allocate the whole block
    int jstage;
    rizz_shader_stage stage = _RIZZ_SHADER_STAGE_COUNT;
    if ((jstage = cj5_seek(&jres, 0, "vs")) != -1) {
        stage = RIZZ_SHADER_STAGE_VS;
    } else if ((jstage = cj5_seek(&jres, 0, "fs")) != -1) {
        stage = RIZZ_SHADER_STAGE_FS;
    } else if ((jstage = cj5_seek(&jres, 0, "cs")) != -1) {
        stage = RIZZ_SHADER_STAGE_CS;
    }

    if (stage == _RIZZ_SHADER_STAGE_COUNT || stage == RIZZ_SHADER_STAGE_CS) {
        rizz__log_error("loading shader reflection failed: there are no valid stages");
        return NULL;
    }

    int jinputs = -1;
    int num_inputs = 0, num_uniforms = 0, num_textures = 0, num_storage_images = 0, num_storage_buffers = 0;
    int juniforms, jtextures, jstorage_images, jstorage_buffers;

    if (stage == RIZZ_SHADER_STAGE_VS) {
        jinputs = cj5_seek(&jres, jstage, "inputs");
        if (jinputs != -1) {
            num_inputs = jres.tokens[jinputs].size;
        }
    }

    if ((juniforms = cj5_seek(&jres, jstage, "uniform_buffers")) != -1) {
        num_uniforms = jres.tokens[juniforms].size;
    }

    if ((jtextures = cj5_seek(&jres, jstage, "textures")) != -1) {
        num_textures = jres.tokens[jtextures].size;
    }

    if ((jstorage_images = cj5_seek(&jres, jstage, "storage_images")) != -1) {
        num_storage_images = jres.tokens[jstorage_images].size;
    }

    if ((jstorage_buffers = cj5_seek(&jres, jstage, "storage_buffers")) != -1) {
        num_storage_buffers = jres.tokens[jstorage_buffers].size;
    }

    int total_sz = sizeof(rizz_shader_refl) + sizeof(rizz_shader_refl_input) * num_inputs +
                   sizeof(rizz_shader_refl_uniform_buffer) * num_uniforms +
                   sizeof(rizz_shader_refl_texture) * num_textures +
                   sizeof(rizz_shader_refl_texture) * num_storage_buffers +
                   sizeof(rizz_shader_refl_buffer) * num_storage_buffers;

    rizz_shader_refl* refl = (rizz_shader_refl*)sx_calloc(alloc, total_sz);
    if (!refl) {
        sx_out_of_memory();
        return NULL;
    }

    char tmpstr[128];
    refl->lang = rizz__shader_str_to_lang(cj5_seekget_string(&jres, 0, "language", tmpstr, sizeof(tmpstr), ""));
    refl->stage = stage;
    refl->profile_version = cj5_seekget_int(&jres, 0, "profile_version", 0);
    refl->code_type = cj5_seekget_bool(&jres, 0, "bytecode", false) ? RIZZ_SHADER_CODE_BYTECODE
                                                                    : RIZZ_SHADER_CODE_SOURCE;
    refl->flatten_ubos = cj5_seekget_bool(&jres, 0, "flatten_ubos", false);
    char filepath[RIZZ_MAX_PATH];
    sx_os_path_basename(refl->source_file, sizeof(refl->source_file),
                        cj5_seekget_string(&jres, jstage, "file", filepath, sizeof(filepath), ""));

    void* buff = refl + 1;
    if (jinputs != -1) {
        refl->inputs = (rizz_shader_refl_input*)buff;
        rizz_shader_refl_input* input = refl->inputs;
        int jinput = 0;
        for (int i = 0; i < jres.tokens[jinputs].size; i++) {
            jinput = cj5_get_array_elem_incremental(&jres, jinputs, i, jinput);
            cj5_seekget_string(&jres, jinput, "name", input->name, sizeof(input->name), "");
            cj5_seekget_string(&jres, jinput, "semantic", input->semantic, sizeof(input->semantic), "");
            input->semantic_index = cj5_seekget_int(&jres, jinput, "semantic_index", 0);
            input->type = rizz__shader_str_to_vertex_format(
                cj5_seekget_string(&jres, jinput, "type", tmpstr, sizeof(tmpstr), ""));
            ++input;
        }
        refl->num_inputs = num_inputs;
        buff = input;
    }

    if (juniforms != -1) {
        refl->uniform_buffers = (rizz_shader_refl_uniform_buffer*)buff;
        rizz_shader_refl_uniform_buffer* ubo = refl->uniform_buffers;
        int jubo = 0;
        for (int i = 0; i < num_uniforms; i++) {
            jubo = cj5_get_array_elem_incremental(&jres, juniforms, i, jubo);
            cj5_seekget_string(&jres, jubo, "name", ubo->name, sizeof(ubo->name), "");
            ubo->size_bytes = cj5_seekget_int(&jres, jubo, "block_size", 0);
            ubo->binding = cj5_seekget_int(&jres, jubo, "binding", 0);
            ubo->array_size = cj5_seekget_int(&jres, jubo, "array", 1);
            if (ubo->array_size > 1)
                sx_assertf(refl->flatten_ubos,
                          "arrayed uniform buffers should only be generated with --flatten-ubos");
            ++ubo;
        }
        refl->num_uniform_buffers = num_uniforms;
        buff = ubo;
    }

    if (jtextures != -1) {
        refl->textures = (rizz_shader_refl_texture*)buff;
        rizz_shader_refl_texture* tex = refl->textures;
        int jtex = 0;
        for (int i = 0; i < num_textures; i++) {
            jtex = cj5_get_array_elem_incremental(&jres, jtextures, i, jtex);
            cj5_seekget_string(&jres, jtex, "name", tex->name, sizeof(tex->name), "");
            tex->binding = cj5_seekget_int(&jres, jtex, "binding", 0);
            tex->type = rizz__shader_str_to_texture_type(
                cj5_seekget_string(&jres, jtex, "dimension", tmpstr, sizeof(tmpstr), ""),
                cj5_seekget_bool(&jres, jtex, "array", false));
            ++tex;
        }
        refl->num_textures = num_textures;
        buff = tex;
    }

    if (jstorage_images != -1) {
        refl->storage_images = (rizz_shader_refl_texture*)buff;
        rizz_shader_refl_texture* img = refl->storage_images;
        int jstorage_img = 0;
        for (int i = 0; i < num_storage_images; i++) {
            jstorage_img = cj5_get_array_elem_incremental(&jres, jstorage_images, i, jstorage_img);
            cj5_seekget_string(&jres, jstorage_img, "name", img->name, sizeof(img->name), "");
            img->binding = cj5_seekget_int(&jres, jstorage_img, "binding", 0);
            img->type = rizz__shader_str_to_texture_type(
                cj5_seekget_string(&jres, jstorage_img, "dimension", tmpstr, sizeof(tmpstr), ""),
                cj5_seekget_bool(&jres, jstorage_img, "array", false));
            ++img;
        }
        refl->num_storage_images = num_storage_images;
        buff = img;
    }

    if (jstorage_buffers != -1) {
        refl->storage_buffers = (rizz_shader_refl_buffer*)buff;
        rizz_shader_refl_buffer* sbuf = refl->storage_buffers;
        int jstorage_buf = 0;
        for (int i = 0; i < num_storage_buffers; i++) {
            jstorage_buf = cj5_get_array_elem_incremental(&jres, jstorage_buffers, i, jstorage_buf);
            cj5_seekget_string(&jres, jstorage_buf, "name", sbuf->name, sizeof(sbuf->name), "");
            sbuf->size_bytes = cj5_seekget_int(&jres, jstorage_buf, "block_size", 0);
            sbuf->binding = cj5_seekget_int(&jres, jstorage_buf, "binding", 0);
            sbuf->array_stride = cj5_seekget_int(&jres, jstorage_buf, "unsized_array_stride", 1);
            ++sbuf;
        }
        refl->num_uniform_buffers = num_uniforms;
        buff = sbuf;
    }

    if (jres.tokens != tokens) {
        sx_free(alloc, (cj5_token*)jres.tokens);
    }

    return refl;
}

static void rizz__shader_free_reflect(rizz_shader_refl* refl, const sx_alloc* alloc)
{
    sx_assert(refl);
    sx_free(alloc, refl);
}

typedef struct {
    const rizz_shader_refl* refl;
    const void* code;
    int code_size;
} rizz__shader_setup_desc_stage;

static sg_shader_desc* rizz__shader_setup_desc(sg_shader_desc* desc,
                                               const rizz_shader_refl* vs_refl, const void* vs,
                                               int vs_size, const rizz_shader_refl* fs_refl,
                                               const void* fs, int fs_size, uint32_t* name_hdl)
{
    sx_memset(desc, 0x0, sizeof(sg_shader_desc));
    const int num_stages = 2;
    rizz__shader_setup_desc_stage stages[] = {
        { .refl = vs_refl, .code = vs, .code_size = vs_size },
        { .refl = fs_refl, .code = fs, .code_size = fs_size }
    };

    if (name_hdl) {
        desc->label = the__core.str_alloc(name_hdl, fs_refl->source_file);
    }

    for (int i = 0; i < num_stages; i++) {
        const rizz__shader_setup_desc_stage* stage = &stages[i];
        sg_shader_stage_desc* stage_desc = NULL;
        // clang-format off
        switch (stage->refl->stage) {
        case RIZZ_SHADER_STAGE_VS:   
            stage_desc = &desc->vs;
            stage_desc->d3d11_target = "vs_5_0";
            break;
        case RIZZ_SHADER_STAGE_FS:   
            stage_desc = &desc->fs;
            stage_desc->d3d11_target = "ps_5_0";
            break;
        default:
            sx_assertf(0, "not implemented");   
            break;
        }
        // clang-format on

        #if SX_PLATFORM_APPLE
            stage_desc->entry = "main0";
        #endif

        if (stage->refl->code_type == RIZZ_SHADER_CODE_BYTECODE) {
            stage_desc->byte_code = (const uint8_t*)stage->code;
            stage_desc->byte_code_size = stage->code_size;
        } else if (stage->refl->code_type == RIZZ_SHADER_CODE_SOURCE) {
            stage_desc->source = (const char*)stage->code;
        }

        // attributes
        if (stage->refl->stage == RIZZ_SHADER_STAGE_VS) {
            for (int a = 0; a < vs_refl->num_inputs; a++) {
                desc->attrs[a].name = vs_refl->inputs[a].name;
                desc->attrs[a].sem_name = vs_refl->inputs[a].semantic;
                desc->attrs[a].sem_index = vs_refl->inputs[a].semantic_index;
            }
        }

        // uniform blocks
        for (int iub = 0; iub < stage->refl->num_uniform_buffers; iub++) {
            rizz_shader_refl_uniform_buffer* rub = &stage->refl->uniform_buffers[iub];
            sg_shader_uniform_block_desc* ub = &stage_desc->uniform_blocks[rub->binding];
            ub->size = rub->size_bytes;
            if (stage->refl->flatten_ubos) {
                ub->uniforms[0].array_count = rub->array_size;
                ub->uniforms[0].name = rub->name;
                ub->uniforms[0].type = SG_UNIFORMTYPE_FLOAT4;
            }

            // NOTE: individual uniform names are supported by reflection json
            //       But we are not parsing and using them here, because the d3d/metal shaders don't
            //       need them And for GL/GLES, we always flatten them
        }    // foreach uniform-block

        for (int itex = 0; itex < stage->refl->num_textures; itex++) {
            rizz_shader_refl_texture* rtex = &stage->refl->textures[itex];
            sg_shader_image_desc* img = &stage_desc->images[rtex->binding];
            img->name = rtex->name;
            img->type = rtex->type;
        }
    }
    return desc;
}

static sg_shader_desc* rizz__shader_setup_desc_cs(sg_shader_desc* desc,
                                                  const rizz_shader_refl* cs_refl, const void* cs,
                                                  int cs_size, uint32_t* name_hdl)
{
    sx_memset(desc, 0x0, sizeof(sg_shader_desc));
    const int num_stages = 1;
    rizz__shader_setup_desc_stage stages[] = {
        { .refl = cs_refl, .code = cs, .code_size = cs_size }
    };

    if (name_hdl) {
        desc->label = the__core.str_alloc(name_hdl, cs_refl->source_file);
    }

    for (int i = 0; i < num_stages; i++) {
        const rizz__shader_setup_desc_stage* stage = &stages[i];
        sg_shader_stage_desc* stage_desc = NULL;
        switch (stage->refl->stage) {
        case RIZZ_SHADER_STAGE_CS:   
            stage_desc = &desc->cs;
            stage_desc->d3d11_target = "cs_5_0";
            break;
        default:                     
            sx_assertf(0, "not implemented");
            break;
        }

        #if SX_PLATFORM_APPLE
            stage_desc->entry = "main0";
        #endif

        if (stage->refl->code_type == RIZZ_SHADER_CODE_BYTECODE) {
            stage_desc->byte_code = (const uint8_t*)stage->code;
            stage_desc->byte_code_size = stage->code_size;
        } else if (stage->refl->code_type == RIZZ_SHADER_CODE_SOURCE) {
            stage_desc->source = (const char*)stage->code;
        }

        // uniform blocks
        for (int iub = 0; iub < stage->refl->num_uniform_buffers; iub++) {
            rizz_shader_refl_uniform_buffer* rub = &stage->refl->uniform_buffers[iub];
            sg_shader_uniform_block_desc* ub = &stage_desc->uniform_blocks[rub->binding];
            ub->size = rub->size_bytes;
            if (stage->refl->flatten_ubos) {
                ub->uniforms[0].array_count = rub->array_size;
                ub->uniforms[0].name = rub->name;
                ub->uniforms[0].type = SG_UNIFORMTYPE_FLOAT4;
            }

            // NOTE: individual uniform names are supported by reflection json
            //       But we are not parsing and using them here, because the d3d/metal shaders don't
            //       need them And for GL/GLES, we always flatten them
        }    // foreach uniform-block

        // textures
        for (int itex = 0; itex < stage->refl->num_textures; itex++) {
            rizz_shader_refl_texture* rtex = &stage->refl->textures[itex];
            sg_shader_image_desc* img = &stage_desc->images[rtex->binding];
            img->name = rtex->name;
            img->type = rtex->type;
        }

        // storage images
        for (int iimg = 0; iimg < stage->refl->num_storage_images; iimg++) {
            rizz_shader_refl_texture* rimg = &stage->refl->storage_images[iimg];
            sg_shader_image_desc* img = &stage_desc->images[rimg->binding];
            img->name = rimg->name;
            img->type = rimg->type;
        }

        // TODO: storage buffers
    }

    return desc;
}

static rizz_shader rizz__shader_make_with_data(const sx_alloc* alloc, uint32_t vs_data_size,
                                               const uint32_t* vs_data, uint32_t vs_refl_size,
                                               const uint32_t* vs_refl_json, uint32_t fs_data_size,
                                               const uint32_t* fs_data, uint32_t fs_refl_size,
                                               const uint32_t* fs_refl_json)
{
    sx_unused(fs_refl_size);
    sx_unused(vs_refl_size);

    sg_shader_desc shader_desc = { 0 };
    rizz_shader_refl* vs_refl =
        rizz__shader_parse_reflect_json(alloc, (const char*)vs_refl_json, (int)vs_refl_size - 1);
    rizz_shader_refl* fs_refl =
        rizz__shader_parse_reflect_json(alloc, (const char*)fs_refl_json, (int)fs_refl_size - 1);

    rizz_shader s = { .shd = the__gfx.make_shader(
                          rizz__shader_setup_desc(&shader_desc, vs_refl, vs_data, (int)vs_data_size,
                                                  fs_refl, fs_data, (int)fs_data_size, &s.info.name_hdl)) };

    s.info.num_inputs = sx_min(vs_refl->num_inputs, SG_MAX_VERTEX_ATTRIBUTES);
    for (int i = 0; i < s.info.num_inputs; i++) {
        s.info.inputs[i] = vs_refl->inputs[i];
    }
    rizz__shader_free_reflect(vs_refl, alloc);
    rizz__shader_free_reflect(fs_refl, alloc);
    return s;
}

static sg_pipeline_desc* rizz__shader_bindto_pipeline_sg(sg_shader shd,
                                                         const rizz_shader_refl_input* inputs,
                                                         int num_inputs, sg_pipeline_desc* desc,
                                                         const rizz_vertex_layout* vl)
{
    sx_assert(vl);
    desc->shader = shd;

    // map offsets in the `vl` to shader inputs
    int index = 0;
    const rizz_vertex_attr* attr = &vl->attrs[0];
    sx_memset(desc->layout.attrs, 0x0, sizeof(sg_vertex_attr_desc) * SG_MAX_VERTEX_ATTRIBUTES);

    while (attr->semantic && index < num_inputs) {
        bool found = false;
        for (int i = 0; i < num_inputs; i++) {
            if (sx_strequal(attr->semantic, inputs[i].semantic) &&
                attr->semantic_idx == inputs[i].semantic_index) {
                found = true;

                desc->layout.attrs[i].offset = attr->offset;
                desc->layout.attrs[i].format =
                    attr->format != SG_VERTEXFORMAT_INVALID ? attr->format : inputs[i].type;
                desc->layout.attrs[i].buffer_index = attr->buffer_index;
                break;
            }
        }

        if (!found) {
            rizz__log_error("vertex attribute '%s%d' does not exist in actual shader inputs",
                            attr->semantic, attr->semantic_idx);
            sx_assert(0);
        }

        ++attr;
        ++index;
    }

    return desc;
}

static const rizz_shader* rizz__shader_get(rizz_asset shader_asset)
{
#if RIZZ_DEV_BUILD
    sx_assert_always(sx_strequal(the__asset.type_name(shader_asset), "shader") && "asset handle is not a shader");
#endif

    const rizz_shader* shd = (const rizz_shader*)the__asset.obj(shader_asset).ptr;
    sx_assertf(shd, "shader is not loaded or missing");
    return shd;
}

static sg_pipeline_desc* rizz__shader_bindto_pipeline(const rizz_shader* shd,
                                                      sg_pipeline_desc* desc,
                                                      const rizz_vertex_layout* vl)
{
    return rizz__shader_bindto_pipeline_sg(shd->shd, shd->info.inputs, shd->info.num_inputs, desc, vl);
}

static rizz__sgs_chunk rizz__sgs_get_iff_chunk(sx_mem_reader* reader, int64_t size, uint32_t fourcc)
{
    int64_t end = (size > 0) ? sx_min(reader->pos + size, reader->top) : reader->top;
    end -= 8;
    if (reader->pos >= end) {
        return (rizz__sgs_chunk){ .pos = -1 };
    }

    uint32_t ch = *((uint32_t*)(reader->data + reader->pos));
    if (ch == fourcc) {
        reader->pos += sizeof(uint32_t);
        uint32_t chunk_size;
        sx_mem_read_var(reader, chunk_size);
        return (rizz__sgs_chunk){ .pos = reader->pos, .size = chunk_size };
    }

    // chunk not found at start position, try to find it in the remaining data by brute-force
    const uint8_t* buff = reader->data;
    for (int64_t offset = reader->pos; offset < end; offset++) {
        ch = *((uint32_t*)(buff + offset));
        if (ch == fourcc) {
            reader->pos = offset + sizeof(uint32_t);
            uint32_t chunk_size;
            sx_mem_read_var(reader, chunk_size);
            return (rizz__sgs_chunk){ .pos = reader->pos, .size = chunk_size };
        }
    }

    return (rizz__sgs_chunk){ .pos = -1 };
}

static rizz_asset_load_data rizz__shader_on_prepare(const rizz_asset_load_params* params,
                                                    const sx_mem_block* mem)
{
    const sx_alloc* alloc = params->alloc ? params->alloc : g_gfx_alloc;

    rizz_shader* shader = sx_calloc(alloc, sizeof(rizz_shader));
    if (!shader) {
        return (rizz_asset_load_data){ .obj = { 0 } };
    }

    rizz_shader_info* info = &shader->info;

    sx_mem_reader reader;
    sx_mem_init_reader(&reader, mem->data, mem->size);

    uint32_t _sgs;
    sx_mem_read_var(&reader, _sgs);
    if (_sgs != SGS_CHUNK) {
        sx_assertf(0, "invalid sgs file format");
        return (rizz_asset_load_data){ .obj = { 0 } };
    }
    sx_mem_seekr(&reader, sizeof(uint32_t), SX_WHENCE_CURRENT);

    struct sgs_chunk sinfo;
    sx_mem_read_var(&reader, sinfo);

    // read stages
    rizz__sgs_chunk stage_chunk = rizz__sgs_get_iff_chunk(&reader, 0, SGS_CHUNK_STAG);
    while (stage_chunk.pos != -1) {
        uint32_t stage_type;
        sx_mem_read_var(&reader, stage_type);

        if (stage_type == SGS_STAGE_VERTEX) {
            // look for reflection chunk
            rizz__sgs_chunk reflect_chunk =
                rizz__sgs_get_iff_chunk(&reader, stage_chunk.size, SGS_CHUNK_REFL);
            if (reflect_chunk.pos != -1) {
                rizz__with_temp_alloc(tmp_alloc) {
                    rizz_shader_refl* refl = rizz__shader_parse_reflect_bin(
                        tmp_alloc, reader.data + reflect_chunk.pos, reflect_chunk.size);
                    sx_memcpy(info->inputs, refl->inputs,
                              sizeof(rizz_shader_refl_input) * refl->num_inputs);
                    info->num_inputs = refl->num_inputs;
                }
            }
        }

        sx_mem_seekr(&reader, stage_chunk.pos + stage_chunk.size, SX_WHENCE_BEGIN);
        stage_chunk = rizz__sgs_get_iff_chunk(&reader, 0, SGS_CHUNK_STAG);
    }

    shader->shd = the__gfx.alloc_shader();
    sx_assert(shader->shd.id);

    return (rizz_asset_load_data){ .obj = { .ptr = shader } };
}

static bool rizz__shader_on_load(rizz_asset_load_data* data, const rizz_asset_load_params* params, const sx_mem_block* mem)
{
    sx_unused(params);
    sx_unused(mem);
    sx_unused(data);

    return true;
}

static void rizz__shader_on_finalize(rizz_asset_load_data* data, const rizz_asset_load_params* params, const sx_mem_block* mem)
{
    sx_unused(params);

    sg_shader_desc shader_desc;

    rizz_shader_refl *vs_refl = NULL, *fs_refl = NULL, *cs_refl = NULL;
    const uint8_t *vs_data = NULL, *fs_data = NULL, *cs_data = NULL;
    int vs_size = 0, fs_size = 0, cs_size = 0;

    sx_mem_reader reader;
    sx_mem_init_reader(&reader, mem->data, mem->size);
    uint32_t _sgs;
    sx_mem_read_var(&reader, _sgs);
    if (_sgs != SGS_CHUNK) {
        sx_assert_alwaysf(0, "invalid shader SGS file");
        return;
    }
    sx_mem_seekr(&reader, sizeof(uint32_t), SX_WHENCE_CURRENT);

    struct sgs_chunk sinfo;
    sx_mem_read_var(&reader, sinfo);

    rizz__with_temp_alloc(tmp_alloc) {
        // read stages
        rizz__sgs_chunk stage_chunk = rizz__sgs_get_iff_chunk(&reader, 0, SGS_CHUNK_STAG);
        while (stage_chunk.pos != -1) {
            uint32_t stage_type;
            sx_mem_read_var(&reader, stage_type);

            rizz_shader_code_type code_type = RIZZ_SHADER_CODE_SOURCE;
            rizz_shader_stage stage;

            rizz__sgs_chunk code_chunk = rizz__sgs_get_iff_chunk(&reader, stage_chunk.size, SGS_CHUNK_CODE);
            if (code_chunk.pos == -1) {
                code_chunk = rizz__sgs_get_iff_chunk(&reader, stage_chunk.size, SGS_CHUNK_DATA);
                if (code_chunk.pos == -1) {
                    sx_assert_alwaysf(0, "neither data or code chunk is found");
                    return;
                }
                code_type = RIZZ_SHADER_CODE_BYTECODE;
            }

            if (stage_type == SGS_STAGE_VERTEX) {
                vs_data = reader.data + code_chunk.pos;
                vs_size = code_chunk.size;
                stage = RIZZ_SHADER_STAGE_VS;
            } else if (stage_type == SGS_STAGE_FRAGMENT) {
                fs_data = reader.data + code_chunk.pos;
                fs_size = code_chunk.size;
                stage = RIZZ_SHADER_STAGE_FS;
            } else if (stage_type == SGS_STAGE_COMPUTE) {
                cs_data = reader.data + code_chunk.pos;
                cs_size = code_chunk.size;
                stage = RIZZ_SHADER_STAGE_CS;
            } else {
                sx_assertf(0, "not implemented");
                stage = _RIZZ_SHADER_STAGE_COUNT;
            }

            // look for reflection chunk
            sx_mem_seekr(&reader, code_chunk.size, SX_WHENCE_CURRENT);
            rizz__sgs_chunk reflect_chunk =
                rizz__sgs_get_iff_chunk(&reader, stage_chunk.size - code_chunk.size, SGS_CHUNK_REFL);
            if (reflect_chunk.pos != -1) {
                rizz_shader_refl* refl = rizz__shader_parse_reflect_bin(
                    tmp_alloc, reader.data + reflect_chunk.pos, reflect_chunk.size);
                refl->lang = rizz__shader_fourcc_to_lang(sinfo.lang);
                refl->stage = stage;
                refl->profile_version = (int)sinfo.profile_ver;
                refl->code_type = code_type;

                if (stage_type == SGS_STAGE_VERTEX) {
                    vs_refl = refl;
                } else if (stage_type == SGS_STAGE_FRAGMENT) {
                    fs_refl = refl;
                } else if (stage_type == SGS_STAGE_COMPUTE) {
                    cs_refl = refl;
                }
                sx_mem_seekr(&reader, reflect_chunk.size, SX_WHENCE_CURRENT);
            }


            sx_mem_seekr(&reader, stage_chunk.pos + stage_chunk.size, SX_WHENCE_BEGIN);
            stage_chunk = rizz__sgs_get_iff_chunk(&reader, 0, SGS_CHUNK_STAG);
        }

        if (cs_refl && cs_data) {
            rizz__shader_setup_desc_cs(&shader_desc, cs_refl, cs_data, cs_size, NULL);
        } else {
            sx_assert(vs_refl && fs_refl);
            rizz__shader_setup_desc(&shader_desc, vs_refl, vs_data, vs_size, fs_refl, fs_data, fs_size, NULL);
        }

        rizz_shader* shader = data->obj.ptr;
        char basename[64];
        shader_desc.label = the__core.str_alloc(&shader->info.name_hdl, 
                                                sx_os_path_basename(basename, sizeof(basename), params->path));

        the__gfx.init_shader(shader->shd, &shader_desc);
    }
}

static void rizz__shader_on_reload(rizz_asset handle, rizz_asset_obj prev_obj, const sx_alloc* alloc)
{
    sx_unused(alloc);

    sg_shader prev_shader = ((rizz_shader*)prev_obj.ptr)->shd;
    rizz_shader* new_shader = (rizz_shader*)the__asset.obj(handle).ptr;
    for (int i = 0, c = sx_array_count(g_gfx.pips); i < c; i++) {
#if defined(SOKOL_METAL)
        const sg_pipeline_desc* _desc = &g_gfx.pips[i].desc;
        sg_pipeline _pip = g_gfx.pips[i].pip;
#else
        const sg_pipeline_desc* _desc = NULL;
        sg_pipeline _pip = g_gfx.pips[i];
#endif
        sg_set_pipeline_shader(_pip, prev_shader, new_shader->shd, &new_shader->info, _desc);
    }
}

static void rizz__shader_on_release(rizz_asset_obj obj, const sx_alloc* alloc)
{
    rizz_shader* shader = obj.ptr;
    sx_assert(shader);

    if (!alloc)
        alloc = g_gfx_alloc;

    if (shader->shd.id)
        the__gfx.destroy_shader(shader->shd);

    if (shader->info.name_hdl) 
        the__core.str_free(shader->info.name_hdl);

    sx_free(alloc, shader);
}

static void rizz__shader_init()
{
    // NOTE: shaders are always forced to load in blocking mode
    the__asset.register_asset_type("shader",
                                   (rizz_asset_callbacks){ .on_prepare = rizz__shader_on_prepare,
                                                           .on_load = rizz__shader_on_load,
                                                           .on_finalize = rizz__shader_on_finalize,
                                                           .on_reload = rizz__shader_on_reload,
                                                           .on_release = rizz__shader_on_release },
                                   NULL, 0, (rizz_asset_obj){ .ptr = NULL },
                                   (rizz_asset_obj){ .ptr = NULL },
                                   RIZZ_ASSET_LOAD_FLAG_WAIT_ON_LOAD);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// @common
static inline void rizz__stage_add_child(rizz_gfx_stage parent, rizz_gfx_stage child)
{
    sx_assert(parent.id);
    sx_assert(child.id);

    rizz__gfx_stage* _parent = &g_gfx.stages[rizz_to_index(parent.id)];
    rizz__gfx_stage* _child = &g_gfx.stages[rizz_to_index(child.id)];
    if (_parent->child.id) {
        rizz__gfx_stage* _first_child = &g_gfx.stages[rizz_to_index(_parent->child.id)];
        _first_child->prev = child;
        _child->next = _parent->child;
    }

    _parent->child = child;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// trace graphics commands

static void rizz__trace_make_buffer(const sg_buffer_desc* desc, sg_buffer result, void* user_data)
{
    sx_unused(user_data);

    if (g_gfx.record_make_commands) {
        const int32_t _cmd = GFX_COMMAND_MAKE_BUFFER;
        sx_mem_write_var(&g_gfx.trace.make_cmds_writer, _cmd);
        sx_mem_write_var(&g_gfx.trace.make_cmds_writer, result);
        sx_mem_write(&g_gfx.trace.make_cmds_writer, desc, sizeof(sg_buffer_desc));
    }

    g_gfx.trace.t.buffer_size += desc->size;
    g_gfx.trace.t.buffer_peak = sx_max(g_gfx.trace.t.buffer_peak, g_gfx.trace.t.buffer_size);

    ++g_gfx.trace.t.num_buffers;
}

static void rizz__trace_make_image(const sg_image_desc* desc, sg_image result, void* user_data)
{
    sx_unused(user_data);
    if (g_gfx.record_make_commands) {
        const int32_t _cmd = GFX_COMMAND_MAKE_IMAGE;
        sx_mem_write_var(&g_gfx.trace.make_cmds_writer, _cmd);
        sx_mem_write_var(&g_gfx.trace.make_cmds_writer, result);
        sx_mem_write(&g_gfx.trace.make_cmds_writer, desc, sizeof(sg_image_desc));
    }

    int bytesize = _sg_is_valid_rendertarget_depth_format(desc->pixel_format) ? 4 :
        _sg_pixelformat_bytesize(desc->pixel_format);
    int pixels = desc->width * desc->height * desc->num_slices;
    int64_t size = (int64_t)pixels * bytesize;

    if (desc->render_target && 
        (_sg_is_valid_rendertarget_color_format(desc->pixel_format) || _sg_is_valid_rendertarget_depth_format(desc->pixel_format))) 
    {
        sx_assert(desc->num_mipmaps == 1);

        g_gfx.trace.t.render_target_size += size;
        g_gfx.trace.t.render_target_peak =
            sx_max(g_gfx.trace.t.render_target_peak, g_gfx.trace.t.render_target_size);
    } else {
        g_gfx.trace.t.texture_size += size;
        g_gfx.trace.t.texture_peak = sx_max(g_gfx.trace.t.texture_peak, g_gfx.trace.t.texture_size);
    }

    ++g_gfx.trace.t.num_images;
}

static void rizz__trace_make_shader(const sg_shader_desc* desc, sg_shader result, void* user_data)
{
    sx_unused(user_data);

    if (g_gfx.record_make_commands) {
        const int32_t _cmd = GFX_COMMAND_MAKE_SHADER;
        sx_mem_write_var(&g_gfx.trace.make_cmds_writer, _cmd);
        sx_mem_write_var(&g_gfx.trace.make_cmds_writer, result);
        sx_mem_write(&g_gfx.trace.make_cmds_writer, desc, sizeof(sg_shader_desc));
    }

    ++g_gfx.trace.t.num_shaders;
}

static void rizz__trace_make_pipeline(const sg_pipeline_desc* desc, sg_pipeline result, void* user_data)
{
    sx_unused(user_data);

    if (g_gfx.record_make_commands) {
        const int32_t _cmd = GFX_COMMAND_MAKE_PIPELINE;
        sx_mem_write_var(&g_gfx.trace.make_cmds_writer, _cmd);
        sx_mem_write_var(&g_gfx.trace.make_cmds_writer, result);
        sx_mem_write(&g_gfx.trace.make_cmds_writer, desc, sizeof(sg_pipeline_desc));
    }

    ++g_gfx.trace.t.num_pipelines;
}

static void rizz__trace_make_pass(const sg_pass_desc* desc, sg_pass result, void* user_data)
{
    sx_unused(user_data);

    if (g_gfx.record_make_commands) {
        const int32_t _cmd = GFX_COMMAND_MAKE_PASS;
        sx_mem_write_var(&g_gfx.trace.make_cmds_writer, _cmd);
        sx_mem_write_var(&g_gfx.trace.make_cmds_writer, result);
        sx_mem_write(&g_gfx.trace.make_cmds_writer, desc, sizeof(sg_pass_desc));
    }

    ++g_gfx.trace.t.num_passes;
}

static void rizz__trace_destroy_buffer(sg_buffer buf_id, void* user_data)
{
    sx_unused(user_data);
    _sg_buffer_t* buf = _sg_lookup_buffer(&_sg.pools, buf_id.id);
    if (buf) {
        g_gfx.trace.t.buffer_size -= buf->cmn.size;
        --g_gfx.trace.t.num_buffers;
    }
}

static void rizz__trace_destroy_image(sg_image img_id, void* user_data)
{
    sx_unused(user_data);
    _sg_image_t* img = _sg_lookup_image(&_sg.pools, img_id.id);
    if (img) {
        if (img->cmn.render_target && _sg_is_valid_rendertarget_color_format(img->cmn.pixel_format) &&
            _sg_is_valid_rendertarget_depth_format(img->cmn.pixel_format)) {
            sx_assert(img->cmn.num_mipmaps == 1);

            int bytesize = _sg_is_valid_rendertarget_depth_format(img->cmn.pixel_format)
                            ? 4
                            : _sg_pixelformat_bytesize(img->cmn.pixel_format);
            int pixels = img->cmn.width * img->cmn.height * img->cmn.num_slices;
            int64_t size = (int64_t)pixels * bytesize;
            g_gfx.trace.t.render_target_size -= size;
        }
        --g_gfx.trace.t.num_images;
    }
}

static void rizz__trace_destroy_shader(sg_shader shd, void* user_data)
{
    sx_unused(user_data);
    if (shd.id) {
        --g_gfx.trace.t.num_shaders;
    }
}

static void rizz__trace_destroy_pipeline(sg_pipeline pip, void* user_data)
{
    sx_unused(user_data);

    if (pip.id) {
        --g_gfx.trace.t.num_pipelines;
    }
}

static void rizz__trace_destroy_pass(sg_pass pass, void* user_data)
{
    sx_unused(user_data);
    if (pass.id) {
        --g_gfx.trace.t.num_passes;
    }
}

static void rizz__trace_begin_pass(sg_pass pass, const sg_pass_action* pass_action, void* user_data)
{
    sx_unused(user_data);
    sx_unused(pass);
    sx_unused(pass_action);
    ++g_gfx.trace.active_trace->num_apply_passes;
}

static void rizz__trace_begin_default_pass(const sg_pass_action* pass_action, int width, int height,
                                           void* user_data)
{
    sx_unused(pass_action);
    sx_unused(width);
    sx_unused(height);
    sx_unused(user_data);
    ++g_gfx.trace.active_trace->num_apply_passes;
}

static void rizz__trace_apply_pipeline(sg_pipeline pip, void* user_data)
{
    sx_unused(user_data);
    sx_unused(pip);
    ++g_gfx.trace.active_trace->num_apply_pipelines;
}

static void rizz__trace_draw(int base_element, int num_elements, int num_instances, void* user_data)
{
    sx_unused(user_data);
    sx_unused(base_element);

    ++g_gfx.trace.active_trace->num_draws;
    g_gfx.trace.active_trace->num_instances += num_instances;
    g_gfx.trace.active_trace->num_elements += num_elements;
}

void rizz__gfx_trace_reset_frame_stats(rizz_gfx_perframe_trace_zone zone)
{
    sx_assert(zone < _RIZZ_GFX_TRACE_COUNT);
    rizz_gfx_perframe_trace_info* pf = &g_gfx.trace.t.pf[zone];
    pf->num_draws = 0;
    pf->num_instances = 0;
    pf->num_elements = 0;
    pf->num_apply_pipelines = 0;
    pf->num_apply_passes = 0;

    g_gfx.trace.active_trace = pf;
}

static void rizz__gfx_collect_garbage(int64_t frame)
{
    // check frames and destroy objects if they are past 1 frame
    // the reason is because the _staged_ API executes commands one frame after their calls:
    //          frame #1
    // <--------------------->
    //      staged->destroy
    //    execute queued cmds |->      frame #2
    //                        <---------------------->
    //

    // buffers
    for (int i = 0, c = sx_array_count(g_gfx.destroy_buffers); i < c; i++) {
        sg_buffer buf_id = g_gfx.destroy_buffers[i];
        _sg_buffer_t* buf = _sg_lookup_buffer(&_sg.pools, buf_id.id);
        if (frame > buf->cmn.used_frame + 1) {
            if (buf->cmn.usage == SG_USAGE_STREAM) {
                for (int ii = 0, cc = sx_array_count(g_gfx.stream_buffs); ii < cc; ii++) {
                    if (g_gfx.stream_buffs[ii].buf.id == buf_id.id) {
                        sx_array_pop(g_gfx.stream_buffs, ii);
                        break;
                    }
                }
            }
            sg_destroy_buffer(buf_id);
            sx_array_pop(g_gfx.destroy_buffers, i);
            i--;
            c--;
        }
    }

    // pipelines
    for (int i = 0, c = sx_array_count(g_gfx.destroy_pips); i < c; i++) {
        sg_pipeline pip_id = g_gfx.destroy_pips[i];
        _sg_pipeline_t* pip = _sg_lookup_pipeline(&_sg.pools, pip_id.id);
        if (frame > pip->cmn.used_frame + 1) {
#if RIZZ_CONFIG_HOT_LOADING
            for (int ii = 0, cc = sx_array_count(g_gfx.pips); ii < cc; ii++) {
#    if defined(SOKOL_METAL)
                sg_pipeline _pip = g_gfx.pips[ii].pip;
#    else
                sg_pipeline _pip = g_gfx.pips[ii];
#    endif
                if (_pip.id == pip_id.id) {
                    sx_array_pop(g_gfx.pips, ii);
                    break;
                }
            }
#endif
            sg_destroy_pipeline(pip_id);
            sx_array_pop(g_gfx.destroy_pips, i);
            i--;
            c--;
        }
    }

    // shaders
    for (int i = 0, c = sx_array_count(g_gfx.destroy_shaders); i < c; i++) {
        _sg_pipeline_t* shd = _sg_lookup_pipeline(&_sg.pools, g_gfx.destroy_shaders[i].id);
        if (shd && frame > shd->cmn.used_frame + 1) {
            sg_destroy_shader(g_gfx.destroy_shaders[i]);
            sx_array_pop(g_gfx.destroy_shaders, i);
            i--;
            c--;
        } else {
            // TODO (FIXME): crash happened where shd became NULL when we reloaded the shaders
            sx_array_pop(g_gfx.destroy_shaders, i);
            i--;
            c--;
        }
    }

    // passes
    for (int i = 0, c = sx_array_count(g_gfx.destroy_passes); i < c; i++) {
        _sg_pass_t* pass = _sg_lookup_pass(&_sg.pools, g_gfx.destroy_passes[i].id);
        if (frame > pass->cmn.used_frame + 1) {
            sg_destroy_pass(g_gfx.destroy_passes[i]);
            sx_array_pop(g_gfx.destroy_passes, i);
            i--;
            c--;
        }
    }

    // images
    for (int i = 0, c = sx_array_count(g_gfx.destroy_images); i < c; i++) {
        _sg_image_t* img = _sg_lookup_image(&_sg.pools, g_gfx.destroy_images[i].id);
        if (frame > img->cmn.used_frame + 1) {
            sg_destroy_image(g_gfx.destroy_images[i]);
            sx_array_pop(g_gfx.destroy_images, i);
            i--;
            c--;
        }
    }
}

static rizz__gfx_cmdbuffer* rizz__gfx_create_command_buffers(const sx_alloc* alloc)
{
    int num_threads = the__core.job_num_threads();
    rizz__gfx_cmdbuffer* cbs = sx_malloc(alloc, sizeof(rizz__gfx_cmdbuffer) * num_threads);
    if (!cbs) {
        sx_out_of_memory();
        return NULL;
    }

    for (int i = 0; i < num_threads; i++) {
        cbs[i] = (rizz__gfx_cmdbuffer){ .alloc = alloc, .index = i };
    }

    return cbs;
}

//
bool rizz__gfx_init(const sx_alloc* alloc, const sg_desc* desc, bool enable_profile)
{
#if SX_PLATFORM_LINUX
    if (flextInit() != GL_TRUE) {
        rizz__log_error("gfx: could not initialize OpenGL");
        return false;
    }
#endif
    g_gfx_alloc = alloc;
    sg_setup(desc);
    g_gfx.enable_profile = enable_profile;
    g_gfx.trace.active_trace = &g_gfx.trace.t.pf[RIZZ_GFX_TRACE_COMMON];

    // command buffers
    g_gfx.cmd_buffers_feed = rizz__gfx_create_command_buffers(alloc);
    g_gfx.cmd_buffers_render = rizz__gfx_create_command_buffers(alloc);

    // trace calls
    {
        sx_mem_init_writer(&g_gfx.trace.make_cmds_writer, alloc, 0);

        g_gfx.trace.hooks = (sg_trace_hooks){ .make_buffer = rizz__trace_make_buffer,
                                              .make_image = rizz__trace_make_image,
                                              .make_shader = rizz__trace_make_shader,
                                              .make_pipeline = rizz__trace_make_pipeline,
                                              .make_pass = rizz__trace_make_pass,
                                              .destroy_buffer = rizz__trace_destroy_buffer,
                                              .destroy_image = rizz__trace_destroy_image,
                                              .destroy_shader = rizz__trace_destroy_shader,
                                              .destroy_pipeline = rizz__trace_destroy_pipeline,
                                              .destroy_pass = rizz__trace_destroy_pass,
                                              .apply_pipeline = rizz__trace_apply_pipeline,
                                              .begin_pass = rizz__trace_begin_pass,
                                              .begin_default_pass = rizz__trace_begin_default_pass,
                                              .draw = rizz__trace_draw };

        g_gfx.record_make_commands = true;
        sg_install_trace_hooks(&g_gfx.trace.hooks);
    }

    rizz__shader_init();
    rizz__texture_init();

    // profiler
    if (enable_profile) {
        if (RMT_USE_D3D11) {
            rmt_BindD3D11((void*)rizz__app_d3d11_device(), (void*)rizz__app_d3d11_device_context());
        } else if (RMT_USE_OPENGL) {
            rmt_BindOpenGL();
        } 
    }

    #ifdef SOKOL_D3D11
        HRESULT hr = _sg.d3d11.ctx->lpVtbl->QueryInterface(_sg.d3d11.ctx, &IID_ID3D11DeviceContext2, (void**)&g_gfx.d3d11_ctx);
        if (SUCCEEDED(hr)) {
            g_gfx.d3d11_has_marker = g_gfx.d3d11_ctx->lpVtbl->IsAnnotationEnabled(g_gfx.d3d11_ctx) ? true : false;
        } else {
            rizz__log_warn("D3D11: feature version 11_2 is not supported");
        }
    #endif

    {   // config
        const rizz_config* conf = the__app.config();
        g_gfx.tex_mgr.default_min_filter = conf->texture_filter_min;
        g_gfx.tex_mgr.default_mag_filter = conf->texture_filter_mag;
        g_gfx.tex_mgr.default_aniso = conf->texture_aniso;
        g_gfx.tex_mgr.default_first_mip = conf->texture_first_mip;
    }

    return true;
}

static void rizz__gfx_destroy_buffers(rizz__gfx_cmdbuffer* cbs)
{
    for (int i = 0, c = the__core.job_num_threads(); i < c; i++) {
        rizz__gfx_cmdbuffer* cb = &cbs[i];
        sx_assert(cb->running_stage.id == 0);
        sx_array_free(cb->alloc, cb->params_buff);
        sx_array_free(cb->alloc, cb->refs);
    }
}

void rizz__gfx_release()
{
    rizz__texture_release();

    // deferred destroys
    rizz__gfx_collect_garbage(the__core.frame_index() + 100);

    sx_array_free(g_gfx_alloc, g_gfx.destroy_buffers);
    sx_array_free(g_gfx_alloc, g_gfx.destroy_images);
    sx_array_free(g_gfx_alloc, g_gfx.destroy_passes);
    sx_array_free(g_gfx_alloc, g_gfx.destroy_pips);
    sx_array_free(g_gfx_alloc, g_gfx.destroy_shaders);
    rizz__gfx_destroy_buffers(g_gfx.cmd_buffers_feed);
    rizz__gfx_destroy_buffers(g_gfx.cmd_buffers_render);
    sx_free(g_gfx_alloc, g_gfx.cmd_buffers_feed);
    sx_free(g_gfx_alloc, g_gfx.cmd_buffers_render);
    sx_array_free(g_gfx_alloc, g_gfx.stream_buffs);
    sx_array_free(g_gfx_alloc, g_gfx.stages);
    sx_array_free(g_gfx_alloc, g_gfx.pips);

    sx_mem_release_writer(&g_gfx.trace.make_cmds_writer);

    // profiler
    if (g_gfx.enable_profile) {
        if (RMT_USE_D3D11) {
            rmt_UnbindD3D11();
        } else if (RMT_USE_OPENGL) {
            rmt_UnbindOpenGL();
        }
    }

    #ifdef SOKOL_D3D11
        if (g_gfx.d3d11_ctx) {
            g_gfx.d3d11_ctx->lpVtbl->Release(g_gfx.d3d11_ctx);
        }
    #endif
    sg_shutdown();
}

void rizz__gfx_update()
{
    rizz__gfx_collect_garbage(the__core.frame_index());
}

void rizz__gfx_commit_gpu()
{
    sg_commit();
    g_gfx.cur_source_loc = (rizz__gfx_source_loc){ 0 };
}

static rizz_gfx_backend rizz__gfx_backend(void)
{
    return (rizz_gfx_backend)sg_query_backend();
}

static bool rizz__gfx_GL_family()
{
    sg_backend backend = sg_query_backend();
    return backend == SG_BACKEND_GLCORE33 || backend == SG_BACKEND_GLES2 ||
           backend == SG_BACKEND_GLES3;
}

static bool rizz__gfx_GLES_family()
{
    sg_backend backend = sg_query_backend();
    return backend == SG_BACKEND_GLES2 || backend == SG_BACKEND_GLES3;
}

SX_INLINE uint8_t* rizz__cb_alloc_params_buff(rizz__gfx_cmdbuffer* cb, int size, int* offset,
                                              const char* file, uint32_t line)
{
    #if !RIZZ_FINAL
        static_assert(sizeof(rizz__gfx_source_loc)%SX_CONFIG_ALLOCATOR_NATURAL_ALIGNMENT == 0, 
                      "un-aligned rizz__gfx_source_loc size");
        size += sizeof(rizz__gfx_source_loc);
    #else
        sx_unused(file);
        sx_unused(line);
    #endif

    if (size == 0) {
        return NULL;
    }

    uint8_t* ptr = sx_array_add(cb->alloc, cb->params_buff,
                                sx_align_mask(size, SX_CONFIG_ALLOCATOR_NATURAL_ALIGNMENT - 1));
    if (!ptr) {
        sx_out_of_memory();
        return NULL;
    }
    *offset = (int)(intptr_t)(ptr - cb->params_buff);

    #if !RIZZ_FINAL
        *((rizz__gfx_source_loc*)ptr) = (rizz__gfx_source_loc){ .file = file, .line = line };
        ptr += sizeof(rizz__gfx_source_loc);
    #endif    

    return ptr;
}

SX_INLINE void rizz__cb_save_source_loc(uint8_t** pbuff)
{
    sx_assert(*pbuff);

    #if !RIZZ_FINAL
        g_gfx.cur_source_loc = *((rizz__gfx_source_loc*)*pbuff);
        (*pbuff) += sizeof(rizz__gfx_source_loc);
    #else
        sx_unused(pbuff);
    #endif
}

static void rizz__cb_begin_profile_sample(const char* name, uint32_t* hash_cache)
{
    rizz__gfx_cmdbuffer* cb = &g_gfx.cmd_buffers_feed[the__core.job_thread_index()];

    sx_assertf(cb->running_stage.id, "draw related calls must come between begin_stage..end_stage");
    sx_assert_alwaysf(cb->cmd_idx < UINT16_MAX, "exceeded maximum number of graphics calls");

    int offset = 0;
    uint8_t* buff = rizz__cb_alloc_params_buff(cb, 32 + sizeof(uint32_t*), &offset, NULL, 0);
    sx_assert_alwaysf(buff, "out of memory");

    rizz__gfx_cmdbuffer_ref ref = { .key =
                                        (((uint32_t)cb->stage_order << 16) | (uint32_t)cb->cmd_idx),
                                    .cmdbuffer_idx = cb->index,
                                    .cmd = GFX_COMMAND_BEGIN_PROFILE,
                                    .params_offset = offset };
    sx_array_push(cb->alloc, cb->refs, ref);

    ++cb->cmd_idx;

    sx_strcpy((char*)buff, 32, name);
    buff += 32;
    *((uint32_t**)buff) = hash_cache;
}

static uint8_t* rizz__cb_run_begin_profile_sample(uint8_t* buff)
{
    rizz__cb_save_source_loc(&buff);

    const char* name = (const char*)buff;
    sx_unused(name);
    buff += 32;
    uint32_t* hash_cache = *((uint32_t**)buff);
    sx_unused(hash_cache);
    buff += sizeof(uint32_t*);
    rmt__begin_gpu_sample(name, hash_cache);
    return buff;
}

static void rizz__cb_end_profile_sample(void)
{
    rizz__gfx_cmdbuffer* cb = &g_gfx.cmd_buffers_feed[the__core.job_thread_index()];

    sx_assertf(cb->running_stage.id, "draw related calls must come between begin_stage..end_stage");
    sx_assert_alwaysf(cb->cmd_idx < UINT16_MAX, "exceeded maximum number of graphics calls");

    rizz__gfx_cmdbuffer_ref ref = { .key =
                                        (((uint32_t)cb->stage_order << 16) | (uint32_t)cb->cmd_idx),
                                    .cmdbuffer_idx = cb->index,
                                    .cmd = GFX_COMMAND_END_PROFILE,
                                    .params_offset = sx_array_count(cb->params_buff) };
    sx_array_push(cb->alloc, cb->refs, ref);

    ++cb->cmd_idx;
}

static uint8_t* rizz__cb_run_end_profile_sample(uint8_t* buff)
{
    rmt__end_gpu_sample();
    return buff;
}

static void rizz__cb_record_begin_stage(const char* name, int name_sz)
{
    sx_assert(name_sz == 32);

    rizz__gfx_cmdbuffer* cb = &g_gfx.cmd_buffers_feed[the__core.job_thread_index()];

    sx_assertf(cb->running_stage.id,
              "draw related calls must come between begin_stage..end_stage");
    sx_assert_alwaysf(cb->cmd_idx < UINT16_MAX, "exceeded maximum number of graphics calls");

    int offset = 0;
    uint8_t* buff = rizz__cb_alloc_params_buff(cb, name_sz, &offset, NULL, 0);

    rizz__gfx_cmdbuffer_ref ref = { .key =
                                        (((uint32_t)cb->stage_order << 16) | (uint32_t)cb->cmd_idx),
                                    .cmdbuffer_idx = cb->index,
                                    .cmd = GFX_COMMAND_STAGE_PUSH,
                                    .params_offset = offset };
    sx_array_push(cb->alloc, cb->refs, ref);

    ++cb->cmd_idx;

    sx_memcpy(buff, name, name_sz);
}

static uint8_t* rizz__cb_run_begin_stage(uint8_t* buff)
{
    rizz__cb_save_source_loc(&buff);
    const char* name = (const char*)buff;
    buff += 32;    // TODO: match this with stage::name

    sx_strcpy(g_gfx.cur_stage_name, sizeof(g_gfx.cur_stage_name), name);
    sg_push_debug_group(name);
    
    #ifdef SOKOL_D3D11
        if (g_gfx.d3d11_has_marker) {
            ID3D11DeviceContext2* ctx = g_gfx.d3d11_ctx;
            wchar_t wname[64];
            _sapp_win32_utf8_to_wide(name, wname, sizeof(wname));
            ctx->lpVtbl->BeginEventInt(ctx, wname, 0);
        }
    #endif
    return buff;
}

static void rizz__cb_record_end_stage()
{
    rizz__gfx_cmdbuffer* cb = &g_gfx.cmd_buffers_feed[the__core.job_thread_index()];

    sx_assertf(cb->running_stage.id, "draw related calls must come between begin_stage..end_stage");
    sx_assert_alwaysf(cb->cmd_idx < UINT16_MAX, "exceeded maximum number of graphics calls");

    rizz__gfx_cmdbuffer_ref ref = { .key =
                                        (((uint32_t)cb->stage_order << 16) | (uint32_t)cb->cmd_idx),
                                    .cmdbuffer_idx = cb->index,
                                    .cmd = GFX_COMMAND_STAGE_POP,
                                    .params_offset = sx_array_count(cb->params_buff) };
    sx_array_push(cb->alloc, cb->refs, ref);

    ++cb->cmd_idx;
}

static uint8_t* rizz__cb_run_end_stage(uint8_t* buff)
{
    g_gfx.cur_stage_name[0] = '\0';
    sg_pop_debug_group();
    
    #ifdef SOKOL_D3D11
        if (g_gfx.d3d11_has_marker) {
            ID3D11DeviceContext2* ctx = g_gfx.d3d11_ctx;
            ctx->lpVtbl->EndEvent(ctx);
        }
    #endif
    return buff;
}

static bool rizz__cb_begin_stage(rizz_gfx_stage stage)
{
    rizz__gfx_cmdbuffer* cb = &g_gfx.cmd_buffers_feed[the__core.job_thread_index()];

    rizz__gfx_stage* _stage;
    const char* stage_name;
    sx_lock(g_gfx.stage_lk) {
        _stage = &g_gfx.stages[rizz_to_index(stage.id)];
        sx_assertf(_stage->state == STAGE_STATE_NONE, "already called begin on this stage");
        bool enabled = _stage->enabled;
        if (!enabled) {
            sx_lock_exit(&g_gfx.stage_lk);
            return false;
        }
        _stage->state = STAGE_STATE_SUBMITTING;
        cb->running_stage = stage;
        cb->stage_order = _stage->order;
        stage_name = _stage->name;
    }

    rizz__cb_record_begin_stage(_stage->name, sizeof(_stage->name));

    char prof_name[64];
    sx_snprintf(prof_name, sizeof(prof_name), "Stage: %s", stage_name);
    rizz__cb_begin_profile_sample(prof_name, NULL);
    
    return true;
}

static void rizz__cb_end_stage()
{
    rizz__gfx_cmdbuffer* cb = &g_gfx.cmd_buffers_feed[the__core.job_thread_index()];
    sx_assertf(cb->running_stage.id, "must call begin_stage before this call");

    rizz__cb_end_profile_sample();

    sx_lock(g_gfx.stage_lk) {
        rizz__gfx_stage* _stage = &g_gfx.stages[rizz_to_index(cb->running_stage.id)];
        sx_assertf(_stage->state == STAGE_STATE_SUBMITTING, "should call begin on this stage first");
        _stage->state = STAGE_STATE_DONE;
    }

    rizz__cb_record_end_stage();
    cb->running_stage = (rizz_gfx_stage){ 0 };
}

static void rizz__cb_begin_default_pass_d(const sg_pass_action* pass_action, int width, int height, 
                                          const char* file, uint32_t line) 
{
    rizz__gfx_cmdbuffer* cb = &g_gfx.cmd_buffers_feed[the__core.job_thread_index()];

    sx_assertf(cb->running_stage.id, "draw related calls must come between begin_stage..end_stage");
    sx_assert_alwaysf(cb->cmd_idx < UINT16_MAX, "exceeded maximum number of graphics calls");

    int offset = 0;
    uint8_t* buff = rizz__cb_alloc_params_buff(cb, sizeof(sg_pass_action) + sizeof(int) * 2, &offset, file, line);
    sx_assert_alwaysf(buff, "out of memory");

    rizz__gfx_cmdbuffer_ref ref = { .key =
                                        (((uint32_t)cb->stage_order << 16) | (uint32_t)cb->cmd_idx),
                                    .cmdbuffer_idx = cb->index,
                                    .cmd = GFX_COMMAND_BEGIN_DEFAULT_PASS,
                                    .params_offset = offset };
    sx_array_push(cb->alloc, cb->refs, ref);

    ++cb->cmd_idx;

    sx_memcpy(buff, pass_action, sizeof(*pass_action));
    buff += sizeof(*pass_action);
    *((int*)buff) = width;
    buff += sizeof(int);
    *((int*)buff) = height;
}

static void rizz__cb_begin_default_pass(const sg_pass_action* pass_action, int width, int height)
{
    rizz__cb_begin_default_pass_d(pass_action, width, height, NULL, 0);
}

static uint8_t* rizz__cb_run_begin_default_pass(uint8_t* buff)
{
    rizz__cb_save_source_loc(&buff);

    sg_pass_action* pass_action = (sg_pass_action*)buff;
    buff += sizeof(sg_pass_action);
    int width = *((int*)buff);
    buff += sizeof(int);
    int height = *((int*)buff);
    buff += sizeof(int);
    sg_begin_default_pass(pass_action, width, height);
    return buff;
}

static void rizz__cb_begin_pass_d(sg_pass pass, const sg_pass_action* pass_action, const char* file, uint32_t line)
{
    rizz__gfx_cmdbuffer* cb = &g_gfx.cmd_buffers_feed[the__core.job_thread_index()];

    sx_assertf(cb->running_stage.id, "draw related calls must come between begin_stage..end_stage");
    sx_assert_alwaysf(cb->cmd_idx < UINT16_MAX, "exceeded maximum number of graphics calls");

    int offset = 0;
    uint8_t* buff = rizz__cb_alloc_params_buff(cb, sizeof(sg_pass_action) + sizeof(sg_pass), &offset, file, line);
    sx_assert_alwaysf(buff, "out of memory");

    rizz__gfx_cmdbuffer_ref ref = { .cmdbuffer_idx = cb->index,
                                    .cmd = GFX_COMMAND_BEGIN_PASS,
                                    .params_offset = offset,
                                    .key = (((uint32_t)cb->stage_order << 16) |
                                            (uint32_t)cb->cmd_idx) };
    sx_array_push(cb->alloc, cb->refs, ref);

    ++cb->cmd_idx;

    sx_memcpy(buff, pass_action, sizeof(*pass_action));
    buff += sizeof(*pass_action);
    *((sg_pass*)buff) = pass;

    _sg_pass_t* _pass = _sg_lookup_pass(&_sg.pools, pass.id);
    _pass->cmn.used_frame = the__core.frame_index();
}

static void rizz__cb_begin_pass(sg_pass pass, const sg_pass_action* pass_action)
{
    rizz__cb_begin_pass_d(pass, pass_action, NULL, 0);
}

static uint8_t* rizz__cb_run_begin_pass(uint8_t* buff)
{
    rizz__cb_save_source_loc(&buff);

    sg_pass_action* pass_action = (sg_pass_action*)buff;
    buff += sizeof(sg_pass_action);
    sg_pass pass = *((sg_pass*)buff);
    buff += sizeof(sg_pass);
    sg_begin_pass(pass, pass_action);
    return buff;
}

static void rizz__cb_apply_viewport_d(int x, int y, int width, int height, bool origin_top_left, const char* file, uint32_t line)
{
    rizz__gfx_cmdbuffer* cb = &g_gfx.cmd_buffers_feed[the__core.job_thread_index()];

    sx_assertf(cb->running_stage.id, "draw related calls must come between begin_stage..end_stage");
    sx_assert_alwaysf(cb->cmd_idx < UINT16_MAX, "exceeded maximum number of graphics calls");

    int offset = 0;
    uint8_t* buff = rizz__cb_alloc_params_buff(cb, sizeof(int) * 4 + sizeof(bool), &offset, file, line);
    sx_assert_alwaysf(buff, "out of memory");

    rizz__gfx_cmdbuffer_ref ref = { .cmdbuffer_idx = cb->index,
                                    .cmd = GFX_COMMAND_APPLY_VIEWPORT,
                                    .params_offset = offset,
                                    .key = (((uint32_t)cb->stage_order << 16) |
                                            (uint32_t)cb->cmd_idx) };
    sx_array_push(cb->alloc, cb->refs, ref);

    ++cb->cmd_idx;

    *((int*)buff) = x;
    buff += sizeof(int);
    *((int*)buff) = y;
    buff += sizeof(int);
    *((int*)buff) = width;
    buff += sizeof(int);
    *((int*)buff) = height;
    buff += sizeof(int);
    *((bool*)buff) = origin_top_left;
}

static void rizz__cb_apply_viewport(int x, int y, int width, int height, bool origin_top_left)
{
    rizz__cb_apply_viewport_d(x, y, width, height, origin_top_left, NULL, 0);
}

static uint8_t* rizz__cb_run_apply_viewport(uint8_t* buff)
{
    rizz__cb_save_source_loc(&buff);

    int x = *((int*)buff);
    buff += sizeof(int);
    int y = *((int*)buff);
    buff += sizeof(int);
    int width = *((int*)buff);
    buff += sizeof(int);
    int height = *((int*)buff);
    buff += sizeof(int);
    bool origin_top_left = *((bool*)buff);
    buff += sizeof(bool);

    sg_apply_viewport(x, y, width, height, origin_top_left);
    return buff;
}

static void rizz__cb_apply_scissor_rect_d(int x, int y, int width, int height, bool origin_top_left, 
                                          const char* file, uint32_t line)
{

    rizz__gfx_cmdbuffer* cb = &g_gfx.cmd_buffers_feed[the__core.job_thread_index()];

    sx_assertf(cb->running_stage.id, "draw related calls must come between begin_stage..end_stage");
    sx_assert_alwaysf(cb->cmd_idx < UINT16_MAX, "exceeded maximum number of graphics calls");

    int offset = 0;
    uint8_t* buff = rizz__cb_alloc_params_buff(cb, sizeof(int) * 4 + sizeof(bool), &offset, file, line);
    sx_assert_alwaysf(buff, "out of memory");

    rizz__gfx_cmdbuffer_ref ref = { .key =
                                        (((uint32_t)cb->stage_order << 16) | (uint32_t)cb->cmd_idx),
                                    .cmdbuffer_idx = cb->index,
                                    .cmd = GFX_COMMAND_APPLY_SCISSOR_RECT,
                                    .params_offset = offset };
    sx_array_push(cb->alloc, cb->refs, ref);

    ++cb->cmd_idx;

    *((int*)buff) = x;
    buff += sizeof(int);
    *((int*)buff) = y;
    buff += sizeof(int);
    *((int*)buff) = width;
    buff += sizeof(int);
    *((int*)buff) = height;
    buff += sizeof(int);
    *((bool*)buff) = origin_top_left;
}

static void rizz__cb_apply_scissor_rect(int x, int y, int width, int height, bool origin_top_left)
{
    rizz__cb_apply_scissor_rect_d(x, y, width, height, origin_top_left, NULL, 0);
}

static uint8_t* rizz__cb_run_apply_scissor_rect(uint8_t* buff)
{
    rizz__cb_save_source_loc(&buff);

    int x = *((int*)buff);
    buff += sizeof(int);
    int y = *((int*)buff);
    buff += sizeof(int);
    int width = *((int*)buff);
    buff += sizeof(int);
    int height = *((int*)buff);
    buff += sizeof(int);
    bool origin_top_left = *((bool*)buff);
    buff += sizeof(bool);

    sg_apply_scissor_rect(x, y, width, height, origin_top_left);
    return buff;
}

static void rizz__cb_apply_pipeline_d(sg_pipeline pip, const char* file, uint32_t line)
{
    rizz__gfx_cmdbuffer* cb = &g_gfx.cmd_buffers_feed[the__core.job_thread_index()];

    sx_assertf(cb->running_stage.id, "draw related calls must come between begin_stage..end_stage");
    sx_assert_alwaysf(cb->cmd_idx < UINT16_MAX, "exceeded maximum number of graphics calls");

    int offset = 0;
    uint8_t* buff = rizz__cb_alloc_params_buff(cb, sizeof(sg_pipeline), &offset, file, line);
    sx_assert_alwaysf(buff, "out of memory");

    rizz__gfx_cmdbuffer_ref ref = { .key = (((uint32_t)cb->stage_order << 16) | (uint32_t)cb->cmd_idx),
                                    .cmdbuffer_idx = cb->index,
                                    .cmd = GFX_COMMAND_APPLY_PIPELINE,
                                    .params_offset = offset };
    sx_array_push(cb->alloc, cb->refs, ref);

    ++cb->cmd_idx;

    *((sg_pipeline*)buff) = pip;

    _sg_pipeline_t* _pip = _sg_lookup_pipeline(&_sg.pools, pip.id);
    sx_assert(_pip);
    _pip->cmn.used_frame = _pip->shader->cmn.used_frame = the__core.frame_index();
}

static void rizz__cb_apply_pipeline(sg_pipeline pip)
{
    rizz__cb_apply_pipeline_d(pip, NULL, 0);
}

static uint8_t* rizz__cb_run_apply_pipeline(uint8_t* buff)
{
    rizz__cb_save_source_loc(&buff);

    sg_pipeline pip_id = *((sg_pipeline*)buff);
    sg_apply_pipeline(pip_id);
    buff += sizeof(sg_pipeline);

    return buff;
}

static void rizz__cb_apply_bindings_d(const sg_bindings* bind, const char* file, uint32_t line)
{
    rizz__gfx_cmdbuffer* cb = &g_gfx.cmd_buffers_feed[the__core.job_thread_index()];

    sx_assertf(cb->running_stage.id, "draw related calls must come between begin_stage..end_stage");
    sx_assert_alwaysf(cb->cmd_idx < UINT16_MAX, "exceeded maximum number of graphics calls");

    int offset = 0;
    uint8_t* buff = rizz__cb_alloc_params_buff(cb, sizeof(sg_bindings), &offset, file, line);
    sx_assert_alwaysf(buff, "out of memory");

    rizz__gfx_cmdbuffer_ref ref = { .key = (((uint32_t)cb->stage_order << 16) | (uint32_t)cb->cmd_idx),
                                    .cmdbuffer_idx = cb->index,
                                    .cmd = GFX_COMMAND_APPLY_BINDINGS,
                                    .params_offset = offset };
    sx_array_push(cb->alloc, cb->refs, ref);

    ++cb->cmd_idx;

    sx_memcpy(buff, bind, sizeof(*bind));

    // frame update
    int64_t frame_idx = the__core.frame_index();
    for (int i = 0; i < SG_MAX_SHADERSTAGE_BUFFERS; i++) {
        if (bind->vertex_buffers[i].id) {
            _sg_buffer_t* vb = _sg_lookup_buffer(&_sg.pools, bind->vertex_buffers[i].id);
            vb->cmn.used_frame = frame_idx;
        } else {
            break;
        }
    }

    if (bind->index_buffer.id) {
        _sg_buffer_t* ib = _sg_lookup_buffer(&_sg.pools, bind->index_buffer.id);
        ib->cmn.used_frame = frame_idx;
    }

    for (int i = 0; i < SG_MAX_SHADERSTAGE_IMAGES; i++) {
        if (bind->vs_images[i].id) {
            _sg_image_t* img = _sg_lookup_image(&_sg.pools, bind->vs_images[i].id);
            img->cmn.used_frame = frame_idx;
        } else {
            break;
        }
    }

    for (int i = 0; i < SG_MAX_SHADERSTAGE_BUFFERS; i++) {
        if (bind->vs_buffers[i].id) {
            _sg_buffer_t* buf = _sg_lookup_buffer(&_sg.pools, bind->vs_buffers[i].id);
            buf->cmn.used_frame = frame_idx;
        } else {
            break;
        }
    }

    for (int i = 0; i < SG_MAX_SHADERSTAGE_IMAGES; i++) {
        if (bind->fs_images[i].id) {
            _sg_image_t* img = _sg_lookup_image(&_sg.pools, bind->fs_images[i].id);
            img->cmn.used_frame = frame_idx;
        } else {
            break;
        }
    }

    for (int i = 0; i < SG_MAX_SHADERSTAGE_BUFFERS; i++) {
        if (bind->fs_buffers[i].id) {
            _sg_buffer_t* buf = _sg_lookup_buffer(&_sg.pools, bind->fs_buffers[i].id);
            buf->cmn.used_frame = frame_idx;
        } else {
            break;
        }
    }

    for (int i = 0; i < SG_MAX_SHADERSTAGE_IMAGES; i++) {
        if (bind->cs_images[i].id) {
            _sg_image_t* img = _sg_lookup_image(&_sg.pools, bind->cs_images[i].id);
            img->cmn.used_frame = frame_idx;
        } else {
            break;
        }
    }

    for (int i = 0; i < SG_MAX_SHADERSTAGE_BUFFERS; i++) {
        if (bind->cs_buffers[i].id) {
            _sg_buffer_t* buf = _sg_lookup_buffer(&_sg.pools, bind->cs_buffers[i].id);
            buf->cmn.used_frame = frame_idx;
        } else {
            break;
        }
    }

    for (int i = 0; i < SG_MAX_SHADERSTAGE_UAVS; i++) {
        if (bind->cs_buffer_uavs[i].id) {
            _sg_buffer_t* buf = _sg_lookup_buffer(&_sg.pools, bind->cs_buffer_uavs[i].id);
            buf->cmn.used_frame = frame_idx;
        } else {
            break;
        }
    }

    for (int i = 0; i < SG_MAX_SHADERSTAGE_UAVS; i++) {
        if (bind->cs_image_uavs[i].id) {
            _sg_image_t* img = _sg_lookup_image(&_sg.pools, bind->cs_image_uavs[i].id);
            img->cmn.used_frame = frame_idx;
        } else {
            break;
        }
    }
}

static void rizz__cb_apply_bindings(const sg_bindings* bind)
{
    rizz__cb_apply_bindings_d(bind, NULL, 0);
}

static uint8_t* rizz__cb_run_apply_bindings(uint8_t* buff)
{
    rizz__cb_save_source_loc(&buff);

    const sg_bindings* bindings = (const sg_bindings*)buff;
    sg_apply_bindings(bindings);
    buff += sizeof(sg_bindings);

    return buff;
}

static void rizz__cb_apply_uniforms_d(sg_shader_stage stage, int ub_index, const void* data, 
                                      int num_bytes, const char* file, uint32_t line)
{
    rizz__gfx_cmdbuffer* cb = &g_gfx.cmd_buffers_feed[the__core.job_thread_index()];

    sx_assertf(cb->running_stage.id, "draw related calls must come between begin_stage..end_stage");
    sx_assert_alwaysf(cb->cmd_idx < UINT16_MAX, "exceeded maximum number of graphics calls");

    int offset = 0;
    uint8_t* buff = 
        rizz__cb_alloc_params_buff(cb, sizeof(sg_shader_stage) + sizeof(int) * 2 + num_bytes, &offset, file, line);
    sx_assert_alwaysf(buff, "out of memory");

    rizz__gfx_cmdbuffer_ref ref = { .key = (((uint32_t)cb->stage_order << 16) | (uint32_t)cb->cmd_idx),
                                    .cmdbuffer_idx = cb->index,
                                    .cmd = GFX_COMMAND_APPLY_UNIFORMS,
                                    .params_offset = offset };
    sx_array_push(cb->alloc, cb->refs, ref);

    ++cb->cmd_idx;

    *((sg_shader_stage*)buff) = stage;
    buff += sizeof(sg_shader_stage);
    *((int*)buff) = ub_index;
    buff += sizeof(int);
    *((int*)buff) = num_bytes;
    buff += sizeof(int);
    sx_memcpy(buff, data, num_bytes);
}

static void rizz__cb_apply_uniforms(sg_shader_stage stage, int ub_index, const void* data, int num_bytes)
{
    rizz__cb_apply_uniforms_d(stage, ub_index, data, num_bytes, NULL, 0);
}

static uint8_t* rizz__cb_run_apply_uniforms(uint8_t* buff)
{
    rizz__cb_save_source_loc(&buff);

    sg_shader_stage stage = *((sg_shader_stage*)buff);
    buff += sizeof(sg_shader_stage);
    int ub_index = *((int*)buff);
    buff += sizeof(int);
    int num_bytes = *((int*)buff);
    buff += sizeof(int);
    sg_apply_uniforms(stage, ub_index, buff, num_bytes);
    buff += num_bytes;
    return buff;
}

static void rizz__cb_draw_d(int base_element, int num_elements, int num_instances, const char* file, uint32_t line)
{
    rizz__gfx_cmdbuffer* cb = &g_gfx.cmd_buffers_feed[the__core.job_thread_index()];

    sx_assertf(cb->running_stage.id, "draw related calls must come between begin_stage..end_stage");
    sx_assert_alwaysf(cb->cmd_idx < UINT16_MAX, "exceeded maximum number of graphics calls");

    int offset = 0;
    uint8_t* buff = rizz__cb_alloc_params_buff(cb, sizeof(int) * 3, &offset, file, line);
    sx_assert_alwaysf(buff, "out of memory");

    rizz__gfx_cmdbuffer_ref ref = { .key = (((uint32_t)cb->stage_order << 16) | (uint32_t)cb->cmd_idx),
                                    .cmdbuffer_idx = cb->index,
                                    .cmd = GFX_COMMAND_DRAW,
                                    .params_offset = offset };
    sx_array_push(cb->alloc, cb->refs, ref);

    ++cb->cmd_idx;

    *((int*)buff) = base_element;
    buff += sizeof(int);
    *((int*)buff) = num_elements;
    buff += sizeof(int);
    *((int*)buff) = num_instances;
}

static void rizz__cb_draw(int base_element, int num_elements, int num_instances)
{
    rizz__cb_draw_d(base_element, num_elements, num_instances, NULL, 0);
}

static uint8_t* rizz__cb_run_draw(uint8_t* buff)
{
    rizz__cb_save_source_loc(&buff);

    int base_element = *((int*)buff);
    buff += sizeof(int);
    int num_elements = *((int*)buff);
    buff += sizeof(int);
    int num_instances = *((int*)buff);
    buff += sizeof(int);
    sg_draw(base_element, num_elements, num_instances);
    return buff;
}

static void rizz__cb_dispatch_d(int thread_group_x, int thread_group_y, int thread_group_z, const char* file, uint32_t line)
{
    rizz__gfx_cmdbuffer* cb = &g_gfx.cmd_buffers_feed[the__core.job_thread_index()];

    sx_assertf(cb->running_stage.id, "draw related calls must come between begin_stage..end_stage");
    sx_assert_alwaysf(cb->cmd_idx < UINT16_MAX, "exceeded maximum number of graphics calls");

    int offset = 0;
    uint8_t* buff = rizz__cb_alloc_params_buff(cb, sizeof(int) * 3, &offset, file, line);
    sx_assert_alwaysf(buff, "out of memory");

    rizz__gfx_cmdbuffer_ref ref = { .key = (((uint32_t)cb->stage_order << 16) | (uint32_t)cb->cmd_idx),
                                    .cmdbuffer_idx = cb->index,
                                    .cmd = GFX_COMMAND_DISPATCH,
                                    .params_offset = offset };
    sx_array_push(cb->alloc, cb->refs, ref);

    ++cb->cmd_idx;

    *((int*)buff) = thread_group_x;
    buff += sizeof(int);
    *((int*)buff) = thread_group_y;
    buff += sizeof(int);
    *((int*)buff) = thread_group_z;
}

static void rizz__cb_dispatch(int thread_group_x, int thread_group_y, int thread_group_z)
{
    rizz__cb_dispatch_d(thread_group_x, thread_group_y, thread_group_z, NULL, 0);
}

static uint8_t* rizz__cb_run_dispatch(uint8_t* buff)
{
    rizz__cb_save_source_loc(&buff);

    int thread_group_x = *((int*)buff);
    buff += sizeof(int);
    int thread_group_y = *((int*)buff);
    buff += sizeof(int);
    int thread_group_z = *((int*)buff);
    buff += sizeof(int);
    sg_dispatch(thread_group_x, thread_group_y, thread_group_z);
    return buff;
}


static void rizz__cb_end_pass_d(const char* file, uint32_t line)
{
    sx_unused(file);
    sx_unused(line);

    rizz__gfx_cmdbuffer* cb = &g_gfx.cmd_buffers_feed[the__core.job_thread_index()];

    int offset = 0;
    rizz__cb_alloc_params_buff(cb, 0, &offset, file, line);

    sx_assertf(cb->running_stage.id, "draw related calls must come between begin_stage..end_stage");
    sx_assert_alwaysf(cb->cmd_idx < UINT16_MAX, "exceeded maximum number of graphics calls");

    rizz__gfx_cmdbuffer_ref ref = { .key = (((uint32_t)cb->stage_order << 16) | (uint32_t)cb->cmd_idx),
                                    .cmdbuffer_idx = cb->index,
                                    .cmd = GFX_COMMAND_END_PASS,
                                    .params_offset = sx_array_count(cb->params_buff) };
    sx_array_push(cb->alloc, cb->refs, ref);

    ++cb->cmd_idx;
}

static void rizz__cb_end_pass(void)
{
    rizz__cb_end_pass_d(NULL, 0);
}

static uint8_t* rizz__cb_run_end_pass(uint8_t* buff)
{
    rizz__cb_save_source_loc(&buff);

    sg_end_pass();
    return buff;
}

static void rizz__cb_update_buffer_d(sg_buffer buf, const void* data_ptr, int data_size, const char* file, uint32_t line)
{
    rizz__gfx_cmdbuffer* cb = &g_gfx.cmd_buffers_feed[the__core.job_thread_index()];

    sx_assertf(cb->running_stage.id, "draw related calls must come between begin_stage..end_stage");
    sx_assert_alwaysf(cb->cmd_idx < UINT16_MAX, "exceeded maximum number of graphics calls");

    int offset = 0;
    uint8_t* buff = rizz__cb_alloc_params_buff(cb, sizeof(sg_buffer) + data_size + sizeof(int), &offset, file, line);
    sx_assert_alwaysf(buff, "out of memory");

    rizz__gfx_cmdbuffer_ref ref = { .key = (((uint32_t)cb->stage_order << 16) | (uint32_t)cb->cmd_idx),
                                    .cmdbuffer_idx = cb->index,
                                    .cmd = GFX_COMMAND_UPDATE_BUFFER,
                                    .params_offset = offset };
    sx_array_push(cb->alloc, cb->refs, ref);

    ++cb->cmd_idx;

    *((sg_buffer*)buff) = buf;
    buff += sizeof(sg_buffer);
    *((int*)buff) = data_size;
    buff += sizeof(int);
    sx_memcpy(buff, data_ptr, data_size);

    _sg_buffer_t* _buff = _sg_lookup_buffer(&_sg.pools, buf.id);
    _buff->cmn.used_frame = the__core.frame_index();
}

static void rizz__cb_update_buffer(sg_buffer buf, const void* data_ptr, int data_size)
{
    rizz__cb_update_buffer_d(buf, data_ptr, data_size, NULL, 0);
}

static uint8_t* rizz__cb_run_update_buffer(uint8_t* buff)
{
    rizz__cb_save_source_loc(&buff);

    sg_buffer buf = *((sg_buffer*)buff);
    buff += sizeof(sg_buffer);
    int data_size = *((int*)buff);
    buff += sizeof(int);
    sg_update_buffer(buf, buff, data_size);
    buff += data_size;

    return buff;
}

static int rizz__cb_append_buffer_d(sg_buffer buf, const void* data_ptr, int data_size, const char* file, uint32_t line)
{
    // search for stream-buffer
    int index = -1;
    for (int i = 0, c = sx_array_count(g_gfx.stream_buffs); i < c; i++) {
        if (g_gfx.stream_buffs[i].buf.id == buf.id) {
            index = i;
            break;
        }
    }

    sx_assertf(index != -1, "buffer must be stream and not destroyed during render");
    rizz__gfx_stream_buffer* sbuff = &g_gfx.stream_buffs[index];
    sx_assert(sbuff->offset + data_size <= sbuff->size);
    int stream_offset = sx_atomic_fetch_add(&sbuff->offset, data_size);

    rizz__gfx_cmdbuffer* cb = &g_gfx.cmd_buffers_feed[the__core.job_thread_index()];

    sx_assertf(cb->running_stage.id, "draw related calls must come between begin_stage..end_stage");
    sx_assert_alwaysf(cb->cmd_idx < UINT16_MAX, "exceeded maximum number of graphics calls");

    int offset = 0;
    uint8_t* buff = rizz__cb_alloc_params_buff(cb, data_size + sizeof(int) * 3 + sizeof(sg_buffer), &offset, file, line);
    sx_assert_alwaysf(buff, "out of memory");

    rizz__gfx_cmdbuffer_ref ref = { .key =
                                        (((uint32_t)cb->stage_order << 16) | (uint32_t)cb->cmd_idx),
                                    .cmdbuffer_idx = cb->index,
                                    .cmd = GFX_COMMAND_APPEND_BUFFER,
                                    .params_offset = offset };
    sx_array_push(cb->alloc, cb->refs, ref);

    ++cb->cmd_idx;

    *((int*)buff) = index;
    buff += sizeof(int);
    *((sg_buffer*)buff) = buf;
    buff += sizeof(sg_buffer);    // keep this for validation
    *((int*)buff) = stream_offset;
    buff += sizeof(int);
    *((int*)buff) = data_size;
    buff += sizeof(int);
    sx_memcpy(buff, data_ptr, data_size);

    _sg_buffer_t* _buff = _sg_lookup_buffer(&_sg.pools, buf.id);
    _buff->cmn.used_frame = the__core.frame_index();

    return stream_offset;
}

static int rizz__cb_append_buffer(sg_buffer buf, const void* data_ptr, int data_size)
{
    return rizz__cb_append_buffer_d(buf, data_ptr, data_size, NULL, 0);
}

static uint8_t* rizz__cb_run_append_buffer(uint8_t* buff)
{
    rizz__cb_save_source_loc(&buff);

    int stream_index = *((int*)buff);
    buff += sizeof(int);
    sg_buffer buf = *((sg_buffer*)buff);
    buff += sizeof(sg_buffer);
    int stream_offset = *((int*)buff);
    buff += sizeof(int);
    int data_size = *((int*)buff);
    buff += sizeof(int);

    sx_assert(stream_index < sx_array_count(g_gfx.stream_buffs));
    sx_assert(g_gfx.stream_buffs);
    rizz__gfx_stream_buffer* sbuff = &g_gfx.stream_buffs[stream_index];
    sx_unused(sbuff);
    sx_assertf(sbuff->buf.id == buf.id, "streaming buffers probably destroyed during render/update");
    sg_map_buffer(buf, stream_offset, buff, data_size);
    buff += data_size;

    return buff;
}

static void rizz__cb_update_image_d(sg_image img, const sg_image_content* data, const char* file, uint32_t line)
{
    rizz__gfx_cmdbuffer* cb = &g_gfx.cmd_buffers_feed[the__core.job_thread_index()];

    sx_assertf(cb->running_stage.id, "draw related calls must come between begin_stage..end_stage");
    sx_assert_alwaysf(cb->cmd_idx < UINT16_MAX, "exceeded maximum number of graphics calls");

    int image_size = 0;
    for (int face = 0; face < SG_CUBEFACE_NUM; face++) {
        for (int mip = 0; mip < SG_MAX_MIPMAPS; mip++) {
            image_size += data->subimage[face][mip].size;
        }
    }

    int offset = 0;
    uint8_t* buff = rizz__cb_alloc_params_buff(cb, sizeof(sg_image) + sizeof(sg_image_content), &offset, file, line);
    sx_assert_alwaysf(buff, "out of memory");

    rizz__gfx_cmdbuffer_ref ref = { .key =
                                        (((uint32_t)cb->stage_order << 16) | (uint32_t)cb->cmd_idx),
                                    .cmdbuffer_idx = cb->index,
                                    .cmd = GFX_COMMAND_UPDATE_IMAGE,
                                    .params_offset = offset };
    sx_array_push(cb->alloc, cb->refs, ref);

    ++cb->cmd_idx;

    *((sg_image*)buff) = img;
    buff += sizeof(sg_image);
    sg_image_content* data_copy = (sg_image_content*)buff;
    buff += sizeof(sg_image_content);
    sx_memset(data_copy, 0x0, sizeof(sg_image_content));
    uint8_t* start_buff = buff;

    for (int face = 0; face < SG_CUBEFACE_NUM; face++) {
        for (int mip = 0; mip < SG_MAX_MIPMAPS; mip++) {
            if (data->subimage[face][mip].ptr) {
                sx_memcpy(buff, data->subimage[face][mip].ptr, data->subimage[face][mip].size);
                data_copy->subimage[face][mip].ptr =
                    (const void*)(buff - start_buff);    // this is actually the offset
                data_copy->subimage[face][mip].size = data->subimage[face][mip].size;

                buff += data->subimage[face][mip].size;
            }
        }
    }

    _sg_image_t* _img = _sg_lookup_image(&_sg.pools, img.id);
    _img->cmn.used_frame = the__core.frame_index();
}

static void rizz__cb_update_image(sg_image img, const sg_image_content* data)
{
    rizz__cb_update_image_d(img, data, NULL, 0);
}

static uint8_t* rizz__cb_run_update_image(uint8_t* buff)
{
    rizz__cb_save_source_loc(&buff);

    sg_image img_id = *((sg_image*)buff);
    buff += sizeof(sg_image);
    sg_image_content data = *((sg_image_content*)buff);
    buff += sizeof(sg_image_content);
    uint8_t* start_buff = buff;

    // change offsets to pointers
    for (int face = 0; face < SG_CUBEFACE_NUM; face++) {
        for (int mip = 0; mip < SG_MAX_MIPMAPS; mip++) {
            if (data.subimage[face][mip].size) {
                data.subimage[face][mip].ptr = start_buff + (intptr_t)data.subimage[face][mip].ptr;
                buff += data.subimage[face][mip].size;
            }
        }
    }

    sg_update_image(img_id, &data);

    return buff;
}

// clang-format off
static const rizz__run_command_cb k_run_cbs[_GFX_COMMAND_COUNT] = {
    rizz__cb_run_begin_default_pass, 
    rizz__cb_run_begin_pass,
    rizz__cb_run_apply_viewport,     
    rizz__cb_run_apply_scissor_rect,
    rizz__cb_run_apply_pipeline,     
    rizz__cb_run_apply_bindings,
    rizz__cb_run_apply_uniforms,     
    rizz__cb_run_draw,
    rizz__cb_run_dispatch,           
    rizz__cb_run_end_pass,
    rizz__cb_run_update_buffer,      
    rizz__cb_run_update_image,
    rizz__cb_run_append_buffer,      
    rizz__cb_run_begin_profile_sample,
    rizz__cb_run_end_profile_sample, 
    rizz__cb_run_begin_stage,
    rizz__cb_run_end_stage
};
// clang-format on

static void rizz__gfx_validate_stage_deps()
{
    sx_lock(g_gfx.stage_lk) {
        for (int i = 0, c = sx_array_count(g_gfx.stages); i < c; i++) {
            rizz__gfx_stage* _stage = &g_gfx.stages[i];
            if (_stage->state == STAGE_STATE_DONE && _stage->parent.id) {
                rizz__gfx_stage* _parent = &g_gfx.stages[rizz_to_index(_stage->parent.id)];
                if (_parent->state != STAGE_STATE_DONE) {
                    sx_assertf(0,
                        "trying to execute stage '%s' that depends on '%s', but '%s' is not rendered",
                        _stage->name, _parent->name, _parent->name);
                }
            }
        }
    }
}

static int rizz__gfx_execute_command_buffer(rizz__gfx_cmdbuffer* cmds)
{
    sx_assertf(the__core.job_thread_index() == 0, "must only be called from main thread");
    static_assert((sizeof(k_run_cbs) / sizeof(rizz__run_command_cb)) == _GFX_COMMAND_COUNT,
                  "k_run_cbs must match rizz__gfx_command");

    // gather all command buffers that submitted a command
    int cmd_count = 0;
    rizz__with_temp_alloc(tmp_alloc) {
        int cmd_buffer_count = the__core.job_num_threads();

        for (int i = 0, c = cmd_buffer_count; i < c; i++) {
            rizz__gfx_cmdbuffer* cb = &cmds[i];
            sx_assertf(cb->running_stage.id == 0,
                      "all command buffers must first fully submit their calls and call end_stage");
            cmd_count += sx_array_count(cb->refs);
        }

        // gather/sort and submit to GPU
        if (cmd_count) {
            rizz__gfx_cmdbuffer_ref* refs =
                sx_malloc(tmp_alloc, sizeof(rizz__gfx_cmdbuffer_ref) * cmd_count);
            sx_assert(refs);

            rizz__gfx_cmdbuffer_ref* init_refs = refs;
            for (int i = 0, c = cmd_buffer_count; i < c; i++) {
                rizz__gfx_cmdbuffer* cb = &cmds[i];
                int ref_count = sx_array_count(cb->refs);
                if (ref_count) {
                    sx_memcpy(refs, cb->refs, sizeof(rizz__gfx_cmdbuffer_ref) * ref_count);
                    refs += ref_count;
                    sx_array_clear(cb->refs);
                }
            }
            refs = init_refs;

            // sort the command refs and execute them
            rizz__gfx_tim_sort(refs, cmd_count);

            for (int i = 0; i < cmd_count; i++) {
                const rizz__gfx_cmdbuffer_ref* ref = &refs[i];
                rizz__gfx_cmdbuffer* cb = &cmds[ref->cmdbuffer_idx];
                k_run_cbs[ref->cmd](&cb->params_buff[ref->params_offset]);
            }

            sx_free(tmp_alloc, refs);
        }

        // reset param buffers
        for (int i = 0, c = cmd_buffer_count; i < c; i++) {
            sx_array_clear(cmds[i].params_buff);
            cmds[i].cmd_idx = 0;
        }
    }

    return cmd_count;
}

void rizz__gfx_execute_command_buffers_final()
{
    rizz__gfx_validate_stage_deps();

    // execute both buffers, because there maybe some commands remaining in the feed and not swapped
    rizz__gfx_execute_command_buffer(g_gfx.cmd_buffers_render);
    rizz__gfx_execute_command_buffer(g_gfx.cmd_buffers_feed);

    // clear all stages
    for (int i = 0, c = sx_array_count(g_gfx.stages); i < c; i++) {
        g_gfx.stages[i].state = STAGE_STATE_NONE;
    }

    // clear stream buffer offsets
    for (int i = 0, c = sx_array_count(g_gfx.stream_buffs); i < c; i++) {
        g_gfx.stream_buffs[i].offset = 0;
    }
}

// Note: presents the `feed` buffer for rendering. must run on main thread
//       we couldn't automate this. because there could be multiple jobs doing rendering and the
//       user should be aware to call this when no other threaded rendering is being done
static void rizz__gfx_swap_command_buffers(void)
{
    sx_assertf(the__core.job_thread_index() == 0, "must be called only from the main thread");

    sx_swap(g_gfx.cmd_buffers_feed, g_gfx.cmd_buffers_render, rizz__gfx_cmdbuffer*);
}

static void rizz__gfx_commit(void)
{
    sx_assertf(the__core.job_thread_index() == 0, "must be called only from the main thread");

    rizz__gfx_validate_stage_deps();

    // render commands should be ready for submition
    if (rizz__gfx_execute_command_buffer(g_gfx.cmd_buffers_render) > 0) {
        rizz__gfx_commit_gpu();    // TODO: test this on iOS/MacOS
    }
}

static rizz_gfx_stage rizz__stage_register(const char* name, rizz_gfx_stage parent_stage)
{
    sx_assert(name);
    sx_assert(parent_stage.id == 0 || parent_stage.id <= (uint32_t)sx_array_count(g_gfx.stages));
    sx_assertf(sx_array_count(g_gfx.stages) < MAX_STAGES, "maximum stages exceeded");

    rizz__gfx_stage _stage = { .name_hash = sx_hash_fnv32_str(name),
                               .parent = parent_stage,
                               .enabled = 1,
                               .single_enabled = 1 };
    sx_strcpy(_stage.name, sizeof(_stage.name), name);

    rizz_gfx_stage stage = { .id = rizz_to_id(sx_array_count(g_gfx.stages)) };

    // dependency order
    // higher 6 bits: depth
    // lower 10 bits: Id
    uint16_t depth = 0;
    if (parent_stage.id) {
        uint16_t parent_depth =
            (g_gfx.stages[rizz_to_index(parent_stage.id)].order >> STAGE_ORDER_DEPTH_BITS) &
            STAGE_ORDER_DEPTH_MASK;
        depth = parent_depth + 1;
    }
    sx_assertf(depth < MAX_DEPTH, "maximum stage dependency depth exceeded");

    _stage.order = ((depth << STAGE_ORDER_DEPTH_BITS) & STAGE_ORDER_DEPTH_MASK) |
                   (uint16_t)(rizz_to_index(stage.id) & STAGE_ORDER_ID_MASK);
    sx_array_push(g_gfx_alloc, g_gfx.stages, _stage);

    // add to dependency graph
    if (parent_stage.id) {
        rizz__stage_add_child(parent_stage, stage);
    }

    return stage;
}

static void rizz__stage_enable(rizz_gfx_stage stage)
{
    sx_assert(stage.id);

    sx_lock(g_gfx.stage_lk) {
        rizz__gfx_stage* _stage = &g_gfx.stages[rizz_to_index(stage.id)];
        _stage->enabled = true;
        _stage->single_enabled = true;

        // apply for children
        for (rizz_gfx_stage child = _stage->child; child.id;
            child = g_gfx.stages[rizz_to_index(child.id)].next) {
            rizz__gfx_stage* _child = &g_gfx.stages[rizz_to_index(child.id)];
            _child->enabled = _child->single_enabled;
        }
    }
}

static void rizz__stage_disable(rizz_gfx_stage stage)
{
    sx_assert(stage.id);

    sx_lock(g_gfx.stage_lk) {
        rizz__gfx_stage* _stage = &g_gfx.stages[rizz_to_index(stage.id)];
        _stage->enabled = false;
        _stage->single_enabled = false;

        // apply for children
        for (rizz_gfx_stage child = _stage->child; child.id;
            child = g_gfx.stages[rizz_to_index(child.id)].next) {
            rizz__gfx_stage* _child = &g_gfx.stages[rizz_to_index(child.id)];
            _child->enabled = false;
        }
    }
}

static bool rizz__stage_isenabled(rizz_gfx_stage stage)
{
    sx_assert(stage.id);

    bool enabled;
    sx_lock(g_gfx.stage_lk) {
        enabled = g_gfx.stages[rizz_to_index(stage.id)].enabled;
    }
    return enabled;
}

static rizz_gfx_stage rizz__stage_find(const char* name)
{
    sx_assert(name);

    uint32_t name_hash = sx_hash_fnv32_str(name);
    sx_lock(g_gfx.stage_lk) {
        for (int i = 0, c = sx_array_count(g_gfx.stages); i < c; i++) {
            if (g_gfx.stages[i].name_hash == name_hash)
                return (rizz_gfx_stage){ .id = rizz_to_id(i) };
        }
    }
    return (rizz_gfx_stage){ .id = 0 };
}

static void rizz__init_pipeline(sg_pipeline pip_id, const sg_pipeline_desc* desc)
{
#if RIZZ_CONFIG_HOT_LOADING
#    if defined(SOKOL_METAL)
    rizz__pip_mtl pip = { .pip = pip_id, .desc = *desc };
    sx_array_push(g_gfx_alloc, g_gfx.pips, pip);
#    else
    sx_array_push(g_gfx_alloc, g_gfx.pips, pip_id);
#    endif
#endif
    sg_init_pipeline(pip_id, desc);
}

static sg_pipeline rizz__make_pipeline(const sg_pipeline_desc* desc)
{
    g_gfx.last_shader_error = false;

    sg_pipeline pip_id = sg_make_pipeline(desc);
#if RIZZ_CONFIG_HOT_LOADING
#    if defined(SOKOL_METAL)
    rizz__pip_mtl pip = { .pip = pip_id, .desc = *desc };
    sx_array_push(g_gfx_alloc, g_gfx.pips, pip);
#    else
    sx_array_push(g_gfx_alloc, g_gfx.pips, pip_id);
#    endif
#endif

    if (g_gfx.last_shader_error) {
        rizz__log_error("in pipeline: %s", desc->label ? desc->label : "[NA]");
        g_gfx.last_shader_error = false;
    }

    return pip_id;
}

static void rizz__destroy_pipeline(sg_pipeline pip_id)
{
    if (pip_id.id) {
        rizz__queue_destroy(g_gfx.destroy_pips, pip_id, g_gfx_alloc);
    }
}

static void rizz__destroy_shader(sg_shader shd_id)
{
    if (shd_id.id) {
        rizz__queue_destroy(g_gfx.destroy_shaders, shd_id, g_gfx_alloc);
    }
}

static void rizz__destroy_pass(sg_pass pass_id)
{
    if (pass_id.id) {
        rizz__queue_destroy(g_gfx.destroy_passes, pass_id, g_gfx_alloc);
    }
}

static void rizz__destroy_image(sg_image img_id)
{
    if (img_id.id) {
        rizz__queue_destroy(g_gfx.destroy_images, img_id, g_gfx_alloc);
    }
}

static void rizz__init_buffer(sg_buffer buf_id, const sg_buffer_desc* desc)
{
    if (desc->usage == SG_USAGE_STREAM) {
        rizz__gfx_stream_buffer sbuff = { .buf = buf_id, .offset = 0, .size = desc->size };
        sx_array_push(g_gfx_alloc, g_gfx.stream_buffs, sbuff);
    }
    sg_init_buffer(buf_id, desc);
}

static sg_buffer rizz__make_buffer(const sg_buffer_desc* desc)
{
    sg_buffer buf_id = sg_make_buffer(desc);
    if (desc->usage == SG_USAGE_STREAM) {
        rizz__gfx_stream_buffer sbuff = { .buf = buf_id, .offset = 0, .size = desc->size };
        sx_array_push(g_gfx_alloc, g_gfx.stream_buffs, sbuff);
    }
    return buf_id;
}

static void rizz__destroy_buffer(sg_buffer buf_id)
{
    if (buf_id.id) {
        rizz__queue_destroy(g_gfx.destroy_buffers, buf_id, g_gfx_alloc);
    }
}

static void rizz__begin_profile_sample(const char* name, uint32_t* hash_cache)
{
    sx_unused(name);
    sx_unused(hash_cache);

    rmt__begin_gpu_sample(name, hash_cache);
}

static void rizz__end_profile_sample()
{
    rmt__end_gpu_sample();
}

static void rizz__internal_state(void** make_cmdbuff, int* make_cmdbuff_sz)
{
    *make_cmdbuff = g_gfx.trace.make_cmds_writer.data;
    *make_cmdbuff_sz = (int)g_gfx.trace.make_cmds_writer.pos;
    g_gfx.record_make_commands = false;
}

static const rizz_gfx_trace_info* rizz__trace_info()
{
    return &g_gfx.trace.t;
}

static bool rizz__imm_begin_stage(rizz_gfx_stage stage)
{
    rizz__gfx_stage* _stage;
    const char* stage_name;
    sx_lock(g_gfx.stage_lk) {
        _stage = &g_gfx.stages[rizz_to_index(stage.id)];
        sx_assertf(_stage->state == STAGE_STATE_NONE, "already called begin on this stage");
        bool enabled = _stage->enabled;
        if (!enabled) {
            sx_lock_exit(&g_gfx.stage_lk);
            return false;
        }
        _stage->state = STAGE_STATE_SUBMITTING;
        stage_name = _stage->name;
    }

    sg_push_debug_group(stage_name);

    char prof_name[64];
    sx_snprintf(prof_name, sizeof(prof_name), "Stage: %s", stage_name);
    rmt__begin_gpu_sample(prof_name, NULL);

    #ifdef SOKOL_D3D11
        if (g_gfx.d3d11_has_marker) {
            ID3D11DeviceContext2* ctx = g_gfx.d3d11_ctx;
            wchar_t wname[64];
            _sapp_win32_utf8_to_wide(stage_name, wname, sizeof(wname));
            ctx->lpVtbl->BeginEventInt(ctx, wname, 0);
        }
    #endif

    return true;
}

static void rizz__imm_end_stage() 
{
    rmt__end_gpu_sample();

    sg_pop_debug_group();

    #ifdef SOKOL_D3D11
        if (g_gfx.d3d11_has_marker) {
            ID3D11DeviceContext2* ctx = g_gfx.d3d11_ctx;
            ctx->lpVtbl->EndEvent(ctx);
        }
    #endif
}

void rizz__gfx_log_error(const char* source_file, int line, const char* str)
{
    the__core.print_error(0, source_file, line, str);
}

// clang-format off
rizz_api_gfx the__gfx = {
    .imm = { 
             .begin                 = rizz__imm_begin_stage,
             .end                   = rizz__imm_end_stage,
             .update_buffer         = sg_update_buffer,
             .update_image          = sg_update_image,
             .append_buffer         = sg_append_buffer,
             .begin_default_pass    = sg_begin_default_pass,
             .begin_pass            = sg_begin_pass,
             .apply_viewport        = sg_apply_viewport,
             .apply_scissor_rect    = sg_apply_scissor_rect,
             .apply_pipeline        = sg_apply_pipeline,
             .apply_bindings        = sg_apply_bindings,
             .apply_uniforms        = sg_apply_uniforms,
             .draw                  = sg_draw,
             .dispatch              = sg_dispatch,
             .end_pass              = sg_end_pass,
             .begin_profile_sample  = rizz__begin_profile_sample,
             .end_profile_sample    = rizz__end_profile_sample, 
        },
    .staged = { .begin                = rizz__cb_begin_stage,
                .end                  = rizz__cb_end_stage,
                .begin_default_pass   = rizz__cb_begin_default_pass,
                .begin_pass           = rizz__cb_begin_pass,
                .apply_viewport       = rizz__cb_apply_viewport,
                .apply_scissor_rect   = rizz__cb_apply_scissor_rect,
                .apply_pipeline       = rizz__cb_apply_pipeline,
                .apply_bindings       = rizz__cb_apply_bindings,
                .apply_uniforms       = rizz__cb_apply_uniforms,
                .draw                 = rizz__cb_draw,
                .dispatch             = rizz__cb_dispatch,
                .end_pass             = rizz__cb_end_pass,
                .update_buffer        = rizz__cb_update_buffer,
                .append_buffer        = rizz__cb_append_buffer,
                .update_image         = rizz__cb_update_image,
                .begin_profile_sample = rizz__cb_begin_profile_sample,
                .end_profile_sample   = rizz__cb_end_profile_sample,
                .begin_default_pass_d = rizz__cb_begin_default_pass_d,
                .begin_pass_d         = rizz__cb_begin_pass_d,
                .apply_viewport_d     = rizz__cb_apply_viewport_d,
                .apply_scissor_rect_d = rizz__cb_apply_scissor_rect_d,
                .apply_pipeline_d     = rizz__cb_apply_pipeline_d,
                .apply_bindings_d     = rizz__cb_apply_bindings_d,
                .apply_uniforms_d     = rizz__cb_apply_uniforms_d,
                .draw_d               = rizz__cb_draw_d,
                .dispatch_d           = rizz__cb_dispatch_d,
                .end_pass_d           = rizz__cb_end_pass_d,
                .update_buffer_d      = rizz__cb_update_buffer_d,
                .append_buffer_d      = rizz__cb_append_buffer_d,
                .update_image_d       = rizz__cb_update_image_d,
        },
    .backend                    = rizz__gfx_backend,
    .GL_family                  = rizz__gfx_GL_family,
    .GLES_family                = rizz__gfx_GLES_family,
    .reset_state_cache          = sg_reset_state_cache,
    .present_commands           = rizz__gfx_swap_command_buffers,
    .commit_commands            = rizz__gfx_commit,
    .make_buffer                = rizz__make_buffer,
    .make_image                 = sg_make_image,
    .make_shader                = sg_make_shader,
    .make_pipeline              = rizz__make_pipeline,
    .make_pass                  = sg_make_pass,
    .destroy_buffer             = rizz__destroy_buffer,
    .destroy_image              = rizz__destroy_image,
    .destroy_shader             = rizz__destroy_shader,
    .destroy_pipeline           = rizz__destroy_pipeline,
    .destroy_pass               = rizz__destroy_pass,
    .query_buffer_overflow      = sg_query_buffer_overflow,
    .query_buffer_state         = sg_query_buffer_state,
    .query_image_state          = sg_query_image_state,
    .query_shader_state         = sg_query_shader_state,
    .query_pipeline_state       = sg_query_pipeline_state,
    .query_pass_state           = sg_query_pass_state,
    .query_buffer_defaults      = sg_query_buffer_defaults,
    .query_image_defaults       = sg_query_image_defaults,
    .query_pipeline_defaults    = sg_query_pipeline_defaults,
    .query_pass_defaults        = sg_query_pass_defaults,
    .alloc_buffer               = sg_alloc_buffer,
    .alloc_image                = sg_alloc_image,
    .alloc_shader               = sg_alloc_shader,
    .alloc_pipeline             = sg_alloc_pipeline,
    .alloc_pass                 = sg_alloc_pass,
    .init_buffer                = rizz__init_buffer,
    .init_image                 = sg_init_image,
    .init_shader                = sg_init_shader,
    .init_pipeline              = rizz__init_pipeline,
    .init_pass                  = sg_init_pass,
    .fail_buffer                = sg_fail_buffer,
    .fail_image                 = sg_fail_image,
    .fail_shader                = sg_fail_shader,
    .fail_pipeline              = sg_fail_pipeline,
    .fail_pass                  = sg_fail_pass,
    .setup_context              = sg_setup_context,
    .activate_context           = sg_activate_context,
    .discard_context            = sg_discard_context,
    .install_trace_hooks        = sg_install_trace_hooks,
    .query_desc                 = sg_query_desc,
    .query_buffer_info          = sg_query_buffer_info,
    .query_image_info           = sg_query_image_info,
    .query_shader_info          = sg_query_shader_info,
    .query_pipeline_info        = sg_query_pipeline_info,
    .query_pass_info            = sg_query_pass_info,
    .query_features             = sg_query_features,
    .query_shader_defaults      = sg_query_shader_defaults,
    .query_limits               = sg_query_limits,
    .query_pixelformat          = sg_query_pixelformat,
    .internal_state             = rizz__internal_state,
    .stage_register             = rizz__stage_register,
    .stage_enable               = rizz__stage_enable,
    .stage_disable              = rizz__stage_disable,
    .stage_isenabled            = rizz__stage_isenabled,
    .stage_find                 = rizz__stage_find,
    .shader_parse_reflection    = rizz__shader_parse_reflect_json,
    .shader_free_reflection     = rizz__shader_free_reflect,
    .shader_setup_desc          = rizz__shader_setup_desc,
    .shader_make_with_data      = rizz__shader_make_with_data,
    .shader_bindto_pipeline     = rizz__shader_bindto_pipeline,
    .shader_bindto_pipeline_sg  = rizz__shader_bindto_pipeline_sg,
    .shader_get                 = rizz__shader_get,
    .texture_white              = rizz__texture_white,
    .texture_black              = rizz__texture_black,
    .texture_checker            = rizz__texture_checker,
    .texture_create_checker     = rizz__texture_create_checker,
    .texture_get                = rizz__texture_get,
    .texture_set_default_quality= rizz__texture_set_default_quality,
    .texture_default_quality    = rizz__texture_default_quality,
    .trace_info                 = rizz__trace_info,
};
// clang-format on
