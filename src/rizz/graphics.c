//
// Copyright 2019 Sepehr Taghdisian (septag@github). All rights reserved.
// License: https://github.com/septag/rizz#license-bsd-2-clause
//
// Glossary:
// rizz__texture_xxx: texture management functions (loading, reloading, etc..)
// rizz__shader_xxx: shader management functions (load, reload, reflection, ...)
// rizz__cb_gpu_command: command-buffer main commands
// rizz__cb_run_gpu_command: command-buffer deferred commands (this is actually where the command is
// executed) rizz__gpu_command: overrides for immediate mode commands _sg_xxxx: sokol overrides
//
#include "config.h"

#include "rizz/app.h"
#include "rizz/asset.h"
#include "rizz/core.h"
#include "rizz/reflect.h"

#include "sx/allocator.h"
#include "sx/array.h"
#include "sx/atomic.h"
#include "sx/hash.h"
#include "sx/io.h"
#include "sx/lin-alloc.h"
#include "sx/os.h"
#include "sx/string.h"

#include "sjson/sjson.h"

#include "Remotery.h"

#include <alloca.h>

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
    (g_gfx.enable_profile ? RMT_OPTIONAL(RMT_USE_D3D11, _rmt_BeginD3D11Sample(_name, _hash)) : 0)
#   define rmt__end_gpu_sample()                \
    (g_gfx.enable_profile ? RMT_OPTIONAL(RMT_USE_D3D11, _rmt_EndD3D11Sample()) : 0)
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
    (g_gfx.enable_profile ? RMT_OPTIONAL(RMT_USE_OPENGL, _rmt_BeginOpenGLSample(_name, _hash)) : 0)
#   define rmt__end_gpu_sample()                \
    (g_gfx.enable_profile ? RMT_OPTIONAL(RMT_USE_OPENGL, _rmt_EndOpenGLSample()) : 0)
#else
#   error "Platform graphics is not supported"
#endif

#define SOKOL_MALLOC(s)             sx_malloc(g_gfx_alloc, s)
#define SOKOL_FREE(p)               sx_free(g_gfx_alloc, p)
#define SOKOL_ASSERT(c)             sx_assert(c)
#define SOKOL_LOG(s)                rizz_log_error(s)

SX_PRAGMA_DIAGNOSTIC_PUSH()
SX_PRAGMA_DIAGNOSTIC_IGNORED_CLANG_GCC("-Wunused-function")
SX_PRAGMA_DIAGNOSTIC_IGNORED_CLANG_GCC("-Wunused-variable")
SX_PRAGMA_DIAGNOSTIC_IGNORED_CLANG_GCC("-Wunused-parameter")
#define SOKOL_IMPL
#define SOKOL_API_DECL static
#define SOKOL_API_IMPL static
#define SOKOL_TRACE_HOOKS
#define SOKOL_NO_DEPRECATED
#include "sokol/sokol_gfx.h"
SX_PRAGMA_DIAGNOSTIC_POP();

#define SG_TYPES_ALREADY_DEFINED
#include "rizz/graphics.h"

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

#include rizz_shader_path(shaders_h, debug.vert.h)
#include rizz_shader_path(shaders_h, debug.frag.h)

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

typedef struct rizz__gfx_texture_mgr {
    rizz_texture white_tex;
    rizz_texture black_tex;
    rizz_texture checker_tex;
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
    uint8_t enabled;
    uint8_t single_enabled;
} rizz__gfx_stage;

typedef struct rizz__debug_vertex {
    sx_vec3 pos;
    sx_vec2 uv;
    sx_color color;
} rizz__debug_vertex;

static rizz_vertex_layout k__debug_vertex = {
    .attrs[0] = { .semantic = "POSITION", .offset = offsetof(rizz__debug_vertex, pos) },
    .attrs[1] = { .semantic = "TEXCOORD", .offset = offsetof(rizz__debug_vertex, uv) },
    .attrs[2] = { .semantic = "COLOR",
                  .offset = offsetof(rizz__debug_vertex, color),
                  .format = SG_VERTEXFORMAT_UBYTE4N },
};

typedef struct rizz__debug_uniforms {
    sx_mat4 model;
    sx_mat4 vp;
} rizz__debug_uniforms;

typedef struct rizz__gfx_debug {
    sg_buffer vb;
    sg_buffer ib;
    sg_pipeline pip_wire;
    sg_shader shader;
    sx_mat4 vp;
} rizz__gfx_debug;

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
} rizz__trace_gfx;

typedef struct rizz__gfx {
    rizz__gfx_stage* stages;              // sx_array
    rizz__gfx_cmdbuffer** cmd_buffers;    // sx_array
    sx_lock_t stage_lk;
    rizz__gfx_texture_mgr tex_mgr;
#ifdef SOKOL_METAL
    rizz__pip_mtl* pips;    // sx_array: keep track of pipelines for shader hot-reloads
#else
    sg_pipeline* pips;
#endif
    rizz__gfx_stream_buffer* stream_buffs;    // sx_array: streaming buffers for append_buffers
    rizz__gfx_debug dbg;

    sg_buffer* destroy_buffers;
    sg_shader* destroy_shaders;
    sg_pipeline* destroy_pips;
    sg_pass* destroy_passes;
    sg_image* destroy_images;

    rizz__trace_gfx trace;
    bool enable_profile;
    bool record_make_commands;
} rizz__gfx;


typedef uint8_t* (*rizz__run_command_cb)(uint8_t* buff);

#define SORT_NAME rizz__gfx
#define SORT_TYPE rizz__gfx_cmdbuffer_ref
#define SORT_CMP(x, y) ((x).key < (y).key ? -1 : 1)
SX_PRAGMA_DIAGNOSTIC_PUSH()
SX_PRAGMA_DIAGNOSTIC_IGNORED_MSVC(4267)
SX_PRAGMA_DIAGNOSTIC_IGNORED_MSVC(4244)
SX_PRAGMA_DIAGNOSTIC_IGNORED_MSVC(4146)
SX_PRAGMA_DIAGNOSTIC_IGNORED_CLANG_GCC("-Wunused-function")
SX_PRAGMA_DIAGNOSTIC_IGNORED_CLANG("-Wshorten-64-to-32")
#include "sort/sort.h"
SX_PRAGMA_DIAGNOSTIC_POP()

static rizz__gfx g_gfx;

////////////////////////////////////////////////////////////////////////////////////////////////////
// @sokol_gfx
#if defined(SOKOL_D3D11)
_SOKOL_PRIVATE void _sg_set_pipeline_shader(_sg_pipeline_t* pip, sg_shader shader_id,
                                            _sg_shader_t* shd, const rizz_shader_info* info,
                                            const sg_pipeline_desc* desc)
{
    SOKOL_ASSERT(shd->slot.state == SG_RESOURCESTATE_VALID);
    SOKOL_ASSERT(shd->d3d11_vs_blob && shd->d3d11_vs_blob_length > 0);
    sx_unused(desc);

    pip->shader = shd;
    pip->shader_id = shader_id;
}
#elif defined(SOKOL_METAL)
_SOKOL_PRIVATE void _sg_set_pipeline_shader(_sg_pipeline_t* pip, sg_shader shader_id,
                                            _sg_shader_t* shd, const rizz_shader_info* info,
                                            const sg_pipeline_desc* desc)
{
    sx_unused(info);
    SOKOL_ASSERT(shd->slot.state == SG_RESOURCESTATE_VALID);

    pip->shader = shd;
    pip->shader_id = shader_id;

    // TODO: recreate pipeline descriptor
    SOKOL_ASSERT(pip);
    _sg_mtl_release_resource(_sg.mtl.frame_index, pip->mtl_rps);

    /* create vertex-descriptor */
    MTLVertexDescriptor* vtx_desc = [MTLVertexDescriptor vertexDescriptor];
    int auto_offset[SG_MAX_SHADERSTAGE_BUFFERS];
    for (int layout_index = 0; layout_index < SG_MAX_SHADERSTAGE_BUFFERS; layout_index++) {
        auto_offset[layout_index] = 0;
    }
    /* to use computed offsets, *all* attr offsets must be 0 */
    bool use_auto_offset = true;
    for (int attr_index = 0; attr_index < SG_MAX_VERTEX_ATTRIBUTES; attr_index++) {
        if (desc->layout.attrs[attr_index].offset != 0) {
            use_auto_offset = false;
            break;
        }
    }
    for (int attr_index = 0; attr_index < SG_MAX_VERTEX_ATTRIBUTES; attr_index++) {
        const sg_vertex_attr_desc* a_desc = &desc->layout.attrs[attr_index];
        if (a_desc->format == SG_VERTEXFORMAT_INVALID) {
            break;
        }
        SOKOL_ASSERT((a_desc->buffer_index >= 0) &&
                     (a_desc->buffer_index < SG_MAX_SHADERSTAGE_BUFFERS));
        vtx_desc.attributes[attr_index].format = _sg_mtl_vertex_format(a_desc->format);
        vtx_desc.attributes[attr_index].offset =
            use_auto_offset ? auto_offset[a_desc->buffer_index] : a_desc->offset;
        vtx_desc.attributes[attr_index].bufferIndex = a_desc->buffer_index + SG_MAX_SHADERSTAGE_UBS;
        auto_offset[a_desc->buffer_index] += _sg_vertexformat_bytesize(a_desc->format);
        pip->vertex_layout_valid[a_desc->buffer_index] = true;
    }
    for (int layout_index = 0; layout_index < SG_MAX_SHADERSTAGE_BUFFERS; layout_index++) {
        if (pip->vertex_layout_valid[layout_index]) {
            const sg_buffer_layout_desc* l_desc = &desc->layout.buffers[layout_index];
            const int mtl_vb_slot = layout_index + SG_MAX_SHADERSTAGE_UBS;
            const int stride = l_desc->stride ? l_desc->stride : auto_offset[layout_index];
            SOKOL_ASSERT(stride > 0);
            vtx_desc.layouts[mtl_vb_slot].stride = stride;
            vtx_desc.layouts[mtl_vb_slot].stepFunction =
                _sg_mtl_step_function(_sg_def(l_desc->step_func, SG_VERTEXSTEP_PER_VERTEX));
            vtx_desc.layouts[mtl_vb_slot].stepRate = _sg_def(l_desc->step_rate, 1);
        }
    }

    /* render-pipeline descriptor */
    MTLRenderPipelineDescriptor* rp_desc = [[MTLRenderPipelineDescriptor alloc] init];
    rp_desc.vertexDescriptor = vtx_desc;
    SOKOL_ASSERT(shd->stage[SG_SHADERSTAGE_VS].mtl_func != _SG_MTL_INVALID_SLOT_INDEX);
    rp_desc.vertexFunction = _sg_mtl_idpool[shd->stage[SG_SHADERSTAGE_VS].mtl_func];
    SOKOL_ASSERT(shd->stage[SG_SHADERSTAGE_FS].mtl_func != _SG_MTL_INVALID_SLOT_INDEX);
    rp_desc.fragmentFunction = _sg_mtl_idpool[shd->stage[SG_SHADERSTAGE_FS].mtl_func];
    rp_desc.sampleCount = _sg_def(desc->rasterizer.sample_count, 1);
    rp_desc.alphaToCoverageEnabled = desc->rasterizer.alpha_to_coverage_enabled;
    rp_desc.alphaToOneEnabled = NO;
    rp_desc.rasterizationEnabled = YES;
    rp_desc.depthAttachmentPixelFormat = _sg_mtl_pixel_format(desc->blend.depth_format);
    if (desc->blend.depth_format == SG_PIXELFORMAT_DEPTH_STENCIL) {
        rp_desc.stencilAttachmentPixelFormat = _sg_mtl_pixel_format(desc->blend.depth_format);
    }

    const int att_count = _sg_def(desc->blend.color_attachment_count, 1);
    for (int i = 0; i < att_count; i++) {
        rp_desc.colorAttachments[i].pixelFormat = _sg_mtl_pixel_format(desc->blend.color_format);
        rp_desc.colorAttachments[i].writeMask = _sg_mtl_color_write_mask(
            (sg_color_mask)_sg_def(desc->blend.color_write_mask, SG_COLORMASK_RGBA));
        rp_desc.colorAttachments[i].blendingEnabled = desc->blend.enabled;
        rp_desc.colorAttachments[i].alphaBlendOperation =
            _sg_mtl_blend_op(_sg_def(desc->blend.op_alpha, SG_BLENDOP_ADD));
        rp_desc.colorAttachments[i].rgbBlendOperation =
            _sg_mtl_blend_op(_sg_def(desc->blend.op_rgb, SG_BLENDOP_ADD));
        rp_desc.colorAttachments[i].destinationAlphaBlendFactor =
            _sg_mtl_blend_factor(_sg_def(desc->blend.dst_factor_alpha, SG_BLENDFACTOR_ZERO));
        rp_desc.colorAttachments[i].destinationRGBBlendFactor =
            _sg_mtl_blend_factor(_sg_def(desc->blend.dst_factor_rgb, SG_BLENDFACTOR_ZERO));
        rp_desc.colorAttachments[i].sourceAlphaBlendFactor =
            _sg_mtl_blend_factor(_sg_def(desc->blend.src_factor_alpha, SG_BLENDFACTOR_ONE));
        rp_desc.colorAttachments[i].sourceRGBBlendFactor =
            _sg_mtl_blend_factor(_sg_def(desc->blend.src_factor_rgb, SG_BLENDFACTOR_ONE));
    }
    NSError* err = NULL;
    id<MTLRenderPipelineState> mtl_rps =
        [_sg_mtl_device newRenderPipelineStateWithDescriptor:rp_desc error:&err];
    if (nil == mtl_rps) {
        SOKOL_ASSERT(err);
        SOKOL_LOG([err.localizedDescription UTF8String]);
    }

    pip->mtl_rps = _sg_mtl_add_resource(mtl_rps);
}
#elif defined(SOKOL_GLCORE33) || defined(SOKOL_GLES2) || defined(SOKOL_GLES3)
_SOKOL_PRIVATE void _sg_set_pipeline_shader(_sg_pipeline_t* pip, sg_shader shader_id,
                                            _sg_shader_t* shd, const rizz_shader_info* info,
                                            const sg_pipeline_desc* desc)
{
    SOKOL_ASSERT(shd->slot.state == SG_RESOURCESTATE_VALID);
    sx_unused(desc);

    pip->shader = shd;
    pip->shader_id = shader_id;

    // check that vertex attributes are not changed
    // When vertex attributes change, the required data to re-evaluate attributes will be missing
    // from the program like vertex buffer stride and offsets This scenario should not happen.
    // because the program (cpu side) has to change at the same time, which is not possible
    int num_attrs = info->num_inputs;
    for (int attr_index = 0; attr_index < num_attrs; attr_index++) {
        const rizz_shader_refl_input* in = &info->inputs[attr_index];
        SOKOL_ASSERT(in->name);
        GLint attr_loc = glGetAttribLocation(shd->gl_prog, in->name);
        if (attr_loc != -1) {
            _sg_gl_attr_t* gl_attr = &pip->gl_attrs[attr_loc];
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
    if (pip->shader_id.id == prev_shader_id.id) {
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
        if (buf->map_frame_index != _sg.frame_index) {
            buf->append_pos = 0;
            buf->append_overflow = false;
        }

        if ((offset + num_bytes) > buf->size) {
            buf->append_overflow = true;
        }

        if (buf->slot.state == SG_RESOURCESTATE_VALID) {
            buf->append_pos = offset;    // alter append_pos, so we write at offset
            if (_sg_validate_append_buffer(buf, data, num_bytes)) {
                if (!buf->append_overflow && (num_bytes > 0)) {
                    /* update and append and map on same buffer in same frame not allowed */
                    SOKOL_ASSERT(buf->update_frame_index != _sg.frame_index);
                    SOKOL_ASSERT(buf->append_frame_index != _sg.frame_index);
                    _sg_append_buffer(buf, data, num_bytes,
                                      buf->map_frame_index != _sg.frame_index);
                    buf->map_frame_index = _sg.frame_index;
                }
            }
        }
    } else {
        sx_assert(0 && "invalid buf_id");
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// @texture
static inline sg_image_type rizz__texture_get_type(const ddsktx_texture_info* tc)
{
    sx_assert(!((tc->flags & DDSKTX_TEXTURE_FLAG_CUBEMAP) && (tc->num_layers > 1)) &&
              "cube-array textures are not supported");
    sx_assert(!(tc->num_layers > 1 && tc->depth > 1) && "3d-array textures are not supported");

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

static rizz_asset_load_data rizz__texture_on_prepare(const rizz_asset_load_params* params,
                                                     const void* metadata)
{
    const sx_alloc* alloc = params->alloc ? params->alloc : g_gfx_alloc;
    const rizz_texture_info* info = metadata;

    // TODO: allocate and cache texture data for later restoring (android+ES2)
    rizz_texture* tex = sx_malloc(alloc, sizeof(rizz_texture));
    if (!tex || info->mem_size_bytes == 0) {
        sx_out_of_memory();
        return (rizz_asset_load_data){ .obj = { 0 } };
    }
    tex->img = the__gfx.alloc_image();
    sx_memcpy(&tex->info, info, sizeof(*info));

    return (rizz_asset_load_data){ .obj = { .ptr = tex },
                                   .user = sx_malloc(g_gfx_alloc, sizeof(sg_image_desc)) };
}

static bool rizz__texture_on_load(rizz_asset_load_data* data, const rizz_asset_load_params* params,
                                  const sx_mem_block* mem)
{
    const rizz_texture_load_params* tparams = params->params;
    rizz_texture* tex = data->obj.ptr;
    sg_image_desc* desc = data->user;
    sx_assert(desc);

    *desc = (sg_image_desc){
        .type = tex->info.type,
        .width = tex->info.width,
        .height = tex->info.height,
        .layers = tex->info.layers,
        .num_mipmaps = tex->info.mips,
        .pixel_format = tex->info.format,
        .min_filter = tparams->min_filter,
        .mag_filter = tparams->mag_filter,
        .wrap_u = tparams->wrap_u,
        .wrap_v = tparams->wrap_v,
        .wrap_w = tparams->wrap_w,
    };

    char ext[32];
    sx_os_path_ext(ext, sizeof(ext), params->path);
    if (sx_strequalnocase(ext, ".dds") || sx_strequalnocase(ext, ".ktx")) {
        ddsktx_texture_info tc = { 0 };
        ddsktx_error err;
        if (ddsktx_parse(&tc, mem->data, mem->size, &err)) {
            switch (tex->info.type) {
            case SG_IMAGETYPE_2D: {
                for (int mip = 0; mip < tc.num_mips; mip++) {
                    ddsktx_sub_data sub_data;
                    ddsktx_get_sub(&tc, &sub_data, mem->data, mem->size, 0, 0, mip);
                    desc->content.subimage[0][mip].ptr = sub_data.buff;
                    desc->content.subimage[0][mip].size = sub_data.size_bytes;
                }
            } break;
            case SG_IMAGETYPE_CUBE: {
                for (int face = 0; face < DDSKTX_CUBE_FACE_COUNT; face++) {
                    for (int mip = 0; mip < tc.num_mips; mip++) {
                        ddsktx_sub_data sub_data;
                        ddsktx_get_sub(&tc, &sub_data, mem->data, mem->size, 0, face, mip);
                        desc->content.subimage[face][mip].ptr = sub_data.buff;
                        desc->content.subimage[face][mip].size = sub_data.size_bytes;
                    }
                }
            } break;
            case SG_IMAGETYPE_3D: {
                for (int depth = 0; depth < tc.depth; depth++) {
                    for (int mip = 0; mip < tc.num_mips; mip++) {
                        ddsktx_sub_data sub_data;
                        ddsktx_get_sub(&tc, &sub_data, mem->data, mem->size, 0, depth, mip);
                        desc->content.subimage[depth][mip].ptr = sub_data.buff;
                        desc->content.subimage[depth][mip].size = sub_data.size_bytes;
                    }
                }
            } break;
            case SG_IMAGETYPE_ARRAY: {
                for (int array = 0; array < tc.num_layers; array++) {
                    for (int mip = 0; mip < tc.num_mips; mip++) {
                        ddsktx_sub_data sub_data;
                        ddsktx_get_sub(&tc, &sub_data, mem->data, mem->size, array, 0, mip);
                        desc->content.subimage[array][mip].ptr = sub_data.buff;
                        desc->content.subimage[array][mip].size = sub_data.size_bytes;
                    }
                }
            } break;
            default:
                break;
            }
        } else {
            rizz_log_warn("parsing texture '%s' failed: %s", params->path, err.msg);
            return false;
        }
    } else {
        int w, h, comp;
        stbi_uc* pixels = stbi_load_from_memory(mem->data, mem->size, &w, &h, &comp, 4);
        if (pixels) {
            sx_assert(tex->info.width == w && tex->info.height == h);
            desc->content.subimage[0][0].ptr = pixels;
            desc->content.subimage[0][0].size = w * h * 4;
        } else {
            rizz_log_warn("parsing image '%s' failed: %s", params->path, stbi_failure_reason());
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
    sg_image_desc* desc = data->user;
    sx_assert(desc);

    char ext[32];
    sx_os_path_ext(ext, sizeof(ext), params->path);
    the__gfx.init_image(tex->img, desc);
    if (!sx_strequalnocase(ext, ".dds") && !sx_strequalnocase(ext, ".ktx")) {
        sx_assert(desc->content.subimage[0][0].ptr);
        stbi_image_free((void*)desc->content.subimage[0][0].ptr);
    }

    sx_free(g_gfx_alloc, data->user);

    g_gfx.trace.t.texture_size += tex->info.mem_size_bytes;
    g_gfx.trace.t.texture_peak = sx_max(g_gfx.trace.t.texture_peak, g_gfx.trace.t.texture_size);
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
    sx_free(alloc, tex);
}

static void rizz__texture_on_read_metadata(void* metadata, const rizz_asset_load_params* params,
                                           const sx_mem_block* mem)
{
    rizz_texture_info* info = metadata;
    char ext[32];
    sx_os_path_ext(ext, sizeof(ext), params->path);
    if (sx_strequalnocase(ext, ".dds") || sx_strequalnocase(ext, ".ktx")) {
        ddsktx_texture_info tc = { 0 };
        ddsktx_error err;
        if (ddsktx_parse(&tc, mem->data, mem->size, &err)) {
            info->type = rizz__texture_get_type(&tc);
            info->format = rizz__texture_get_texture_format(tc.format);
            if (info->type == SG_IMAGETYPE_ARRAY)
                info->layers = tc.num_layers;
            else if (info->type == SG_IMAGETYPE_3D)
                info->depth = tc.depth;
            else
                info->layers = 1;
            info->mem_size_bytes = tc.size_bytes;
            info->width = tc.width;
            info->height = tc.height;
            info->mips = tc.num_mips;
            info->bpp = tc.bpp;
        } else {
            rizz_log_warn("reading texture '%s' metadata failed: %s", params->path, err.msg);
            sx_memset(info, 0x0, sizeof(rizz_texture_info));
        }
    } else {
        // try to use stbi to load the image
        int comp;
        if (stbi_info_from_memory(mem->data, mem->size, &info->width, &info->height, &comp)) {
            sx_assert(!stbi_is_16_bit_from_memory(mem->data, mem->size) &&
                      "images with 16bit color channel are not supported");
            info->type = SG_IMAGETYPE_2D;
            info->format = SG_PIXELFORMAT_RGBA8;    // always convert to RGBA
            info->mem_size_bytes = 4 * info->width * info->height;
            info->layers = 1;
            info->mips = 1;
            info->bpp = 32;
        } else {
            rizz_log_warn("reading image '%s' metadata failed: %s", params->path,
                          stbi_failure_reason());
            sx_memset(info, 0x0, sizeof(rizz_texture_info));
        }
    }
}

static rizz_texture rizz__texture_create_checker(int checker_size, int size,
                                                 const sx_color colors[2])
{
    sx_assert(size % 4 == 0 && "size must be multiple of four");
    sx_assert(size % checker_size == 0 && "checker_size must be dividable by size");

    int size_bytes = size * size * sizeof(uint32_t);
    uint32_t* pixels = sx_malloc(g_gfx_alloc, size_bytes);

    // split into tiles and color them
    int tiles_x = size / checker_size;
    int tiles_y = size / checker_size;
    int num_tiles = tiles_x * tiles_y;

    const sx_alloc* tmp_alloc = the__core.tmp_alloc_push();
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

    rizz_texture tex =
        (rizz_texture){ .img = the__gfx.make_image(&(sg_image_desc){
                            .width = size,
                            .height = size,
                            .num_mipmaps = 1,
                            .pixel_format = SG_PIXELFORMAT_RGBA8,
                            .content = (sg_image_content){ .subimage[0][0].ptr = pixels,
                                                           .subimage[0][0].size = size_bytes } }),
                        .info = (rizz_texture_info){ .type = SG_IMAGETYPE_2D,
                                                     .format = SG_PIXELFORMAT_RGBA8,
                                                     .mem_size_bytes = size_bytes,
                                                     .width = size,
                                                     .height = size,
                                                     .layers = 1,
                                                     .mips = 1,
                                                     .bpp = 32 } };

    sx_free(g_gfx_alloc, pixels);
    the__core.tmp_alloc_pop();
    return tex;
}

static void rizz__texture_init()
{
    // register metadata struct reflection
    rizz_refl_enum(sg_image_type, SG_IMAGETYPE_2D);
    rizz_refl_enum(sg_image_type, SG_IMAGETYPE_CUBE);
    rizz_refl_enum(sg_image_type, SG_IMAGETYPE_3D);
    rizz_refl_enum(sg_image_type, SG_IMAGETYPE_ARRAY);
    rizz_refl_enum(sg_pixel_format, SG_PIXELFORMAT_NONE);
    rizz_refl_enum(sg_pixel_format, SG_PIXELFORMAT_R8);
    rizz_refl_enum(sg_pixel_format, SG_PIXELFORMAT_R8SN);
    rizz_refl_enum(sg_pixel_format, SG_PIXELFORMAT_R8UI);
    rizz_refl_enum(sg_pixel_format, SG_PIXELFORMAT_R8SI);
    rizz_refl_enum(sg_pixel_format, SG_PIXELFORMAT_R16);
    rizz_refl_enum(sg_pixel_format, SG_PIXELFORMAT_R16SN);
    rizz_refl_enum(sg_pixel_format, SG_PIXELFORMAT_R16UI);
    rizz_refl_enum(sg_pixel_format, SG_PIXELFORMAT_R16SI);
    rizz_refl_enum(sg_pixel_format, SG_PIXELFORMAT_R16F);
    rizz_refl_enum(sg_pixel_format, SG_PIXELFORMAT_RG8);
    rizz_refl_enum(sg_pixel_format, SG_PIXELFORMAT_RG8SN);
    rizz_refl_enum(sg_pixel_format, SG_PIXELFORMAT_RG8UI);
    rizz_refl_enum(sg_pixel_format, SG_PIXELFORMAT_RG8SI);
    rizz_refl_enum(sg_pixel_format, SG_PIXELFORMAT_R32UI);
    rizz_refl_enum(sg_pixel_format, SG_PIXELFORMAT_R32SI);
    rizz_refl_enum(sg_pixel_format, SG_PIXELFORMAT_R32F);
    rizz_refl_enum(sg_pixel_format, SG_PIXELFORMAT_RG16);
    rizz_refl_enum(sg_pixel_format, SG_PIXELFORMAT_RG16SN);
    rizz_refl_enum(sg_pixel_format, SG_PIXELFORMAT_RG16UI);
    rizz_refl_enum(sg_pixel_format, SG_PIXELFORMAT_RG16SI);
    rizz_refl_enum(sg_pixel_format, SG_PIXELFORMAT_RG16F);
    rizz_refl_enum(sg_pixel_format, SG_PIXELFORMAT_RGBA8);
    rizz_refl_enum(sg_pixel_format, SG_PIXELFORMAT_RGBA8SN);
    rizz_refl_enum(sg_pixel_format, SG_PIXELFORMAT_RGBA8UI);
    rizz_refl_enum(sg_pixel_format, SG_PIXELFORMAT_RGBA8SI);
    rizz_refl_enum(sg_pixel_format, SG_PIXELFORMAT_BGRA8);
    rizz_refl_enum(sg_pixel_format, SG_PIXELFORMAT_RGB10A2);
    rizz_refl_enum(sg_pixel_format, SG_PIXELFORMAT_RG11B10F);
    rizz_refl_enum(sg_pixel_format, SG_PIXELFORMAT_RG32UI);
    rizz_refl_enum(sg_pixel_format, SG_PIXELFORMAT_RG32SI);
    rizz_refl_enum(sg_pixel_format, SG_PIXELFORMAT_RG32F);
    rizz_refl_enum(sg_pixel_format, SG_PIXELFORMAT_RGBA16);
    rizz_refl_enum(sg_pixel_format, SG_PIXELFORMAT_RGBA16SN);
    rizz_refl_enum(sg_pixel_format, SG_PIXELFORMAT_RGBA16UI);
    rizz_refl_enum(sg_pixel_format, SG_PIXELFORMAT_RGBA16SI);
    rizz_refl_enum(sg_pixel_format, SG_PIXELFORMAT_RGBA16F);
    rizz_refl_enum(sg_pixel_format, SG_PIXELFORMAT_RGBA32UI);
    rizz_refl_enum(sg_pixel_format, SG_PIXELFORMAT_RGBA32SI);
    rizz_refl_enum(sg_pixel_format, SG_PIXELFORMAT_RGBA32F);
    rizz_refl_enum(sg_pixel_format, SG_PIXELFORMAT_DEPTH);
    rizz_refl_enum(sg_pixel_format, SG_PIXELFORMAT_DEPTH_STENCIL);
    rizz_refl_enum(sg_pixel_format, SG_PIXELFORMAT_BC1_RGBA);
    rizz_refl_enum(sg_pixel_format, SG_PIXELFORMAT_BC2_RGBA);
    rizz_refl_enum(sg_pixel_format, SG_PIXELFORMAT_BC3_RGBA);
    rizz_refl_enum(sg_pixel_format, SG_PIXELFORMAT_BC4_R);
    rizz_refl_enum(sg_pixel_format, SG_PIXELFORMAT_BC4_RSN);
    rizz_refl_enum(sg_pixel_format, SG_PIXELFORMAT_BC5_RG);
    rizz_refl_enum(sg_pixel_format, SG_PIXELFORMAT_BC5_RGSN);
    rizz_refl_enum(sg_pixel_format, SG_PIXELFORMAT_BC6H_RGBF);
    rizz_refl_enum(sg_pixel_format, SG_PIXELFORMAT_BC6H_RGBUF);
    rizz_refl_enum(sg_pixel_format, SG_PIXELFORMAT_BC7_RGBA);
    rizz_refl_enum(sg_pixel_format, SG_PIXELFORMAT_PVRTC_RGB_2BPP);
    rizz_refl_enum(sg_pixel_format, SG_PIXELFORMAT_PVRTC_RGB_4BPP);
    rizz_refl_enum(sg_pixel_format, SG_PIXELFORMAT_PVRTC_RGBA_2BPP);
    rizz_refl_enum(sg_pixel_format, SG_PIXELFORMAT_PVRTC_RGBA_4BPP);
    rizz_refl_enum(sg_pixel_format, SG_PIXELFORMAT_ETC2_RGB8);
    rizz_refl_enum(sg_pixel_format, SG_PIXELFORMAT_ETC2_RGB8A1);

    rizz_refl_field(rizz_texture_info, sg_image_type, type, "texture type");
    rizz_refl_field(rizz_texture_info, sg_pixel_format, format, "texture pixel format");
    rizz_refl_field(rizz_texture_info, int, mem_size_bytes, "allocated texture size in bytes");
    rizz_refl_field(rizz_texture_info, int, width, "texture width in pixels");
    rizz_refl_field(rizz_texture_info, int, height, "texture height in pixels");
    rizz_refl_field(rizz_texture_info, int, layers, "texture layers/depth");
    rizz_refl_field(rizz_texture_info, int, mips, "number of texture mips");
    rizz_refl_field(rizz_texture_info, int, bpp, "number of bits-per-pixel");

    static uint32_t k_white_pixel = 0xffffffff;
    static uint32_t k_black_pixel = 0xff000000;
    g_gfx.tex_mgr.white_tex = (rizz_texture){
        .img = the__gfx.make_image(&(sg_image_desc){
            .width = 1,
            .height = 1,
            .num_mipmaps = 1,
            .pixel_format = SG_PIXELFORMAT_RGBA8,
            .content = (sg_image_content){ .subimage[0][0].ptr = &k_white_pixel,
                                           .subimage[0][0].size = sizeof(k_white_pixel) } }),
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
                                           .subimage[0][0].size = sizeof(k_black_pixel) } }),
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

    the__asset.register_asset_type(
        "texture",
        (rizz_asset_callbacks){ .on_prepare = rizz__texture_on_prepare,
                                .on_load = rizz__texture_on_load,
                                .on_finalize = rizz__texture_on_finalize,
                                .on_reload = rizz__texture_on_reload,
                                .on_release = rizz__texture_on_release,
                                .on_read_metadata = rizz__texture_on_read_metadata },
        "rizz_texture_load_params", sizeof(rizz_texture_load_params), "rizz_texture_info",
        sizeof(rizz_texture_info), (rizz_asset_obj){ .ptr = &g_gfx.tex_mgr.white_tex },
        (rizz_asset_obj){ .ptr = &g_gfx.tex_mgr.white_tex }, 0);
}

static void rizz__texture_release()
{
    if (g_gfx.tex_mgr.white_tex.img.id)
        the__gfx.destroy_image(g_gfx.tex_mgr.white_tex.img);
    if (g_gfx.tex_mgr.black_tex.img.id)
        the__gfx.destroy_image(g_gfx.tex_mgr.black_tex.img);
    if (g_gfx.tex_mgr.checker_tex.img.id)
        the__gfx.destroy_image(g_gfx.tex_mgr.checker_tex.img);
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
                .size_bytes = u.size_bytes, .binding = u.binding, .array_size = u.array_size
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
                                                         sjson_context* jctx)
{
    // if json context is NULL, create one and close it later
    bool close = false;
    if (!jctx) {
        close = true;
        jctx = sjson_create_context(0, 0, (void*)alloc);
        sx_assert(jctx);
    }
    sjson_node* jroot = sjson_decode(jctx, stage_refl_json);
    if (!jroot) {
        rizz_log_error("loading shader reflection failed: invalid json");
        return NULL;
    }

    // count everything and allocate the whole block
    sjson_node* jstage = NULL;
    rizz_shader_stage stage = _RIZZ_SHADER_STAGE_COUNT;
    if ((jstage = sjson_find_member(jroot, "vs")) != NULL)
        stage = RIZZ_SHADER_STAGE_VS;
    else if ((jstage = sjson_find_member(jroot, "fs")) != NULL)
        stage = RIZZ_SHADER_STAGE_FS;
    else if ((jstage = sjson_find_member(jroot, "cs")) != NULL)
        stage = RIZZ_SHADER_STAGE_CS;

    if (stage == _RIZZ_SHADER_STAGE_COUNT || stage == RIZZ_SHADER_STAGE_CS) {
        rizz_log_error("loading shader reflection failed: there are no valid stages");
        return NULL;
    }

    sjson_node* jinputs = NULL;
    int num_inputs = 0, num_uniforms = 0, num_textures = 0, num_storage_images = 0,
        num_storage_buffers = 0;

    if (stage == RIZZ_SHADER_STAGE_VS) {
        jinputs = sjson_find_member(jstage, "inputs");
        if (jinputs)
            num_inputs = sjson_child_count(jinputs);
    }

    sjson_node* juniforms = sjson_find_member(jstage, "uniform_buffers");
    if (juniforms) {
        num_uniforms = sjson_child_count(juniforms);
    }

    sjson_node* jtextures = sjson_find_member(jstage, "textures");
    if (jtextures) {
        num_textures = sjson_child_count(jtextures);
    }

    sjson_node* jstorage_imgs = sjson_find_member(jstage, "storage_images");
    if (jstorage_imgs) {
        num_storage_images = sjson_child_count(jstorage_imgs);
    }

    sjson_node* jstorage_bufs = sjson_find_member(jstage, "storage_buffers");
    if (jstorage_bufs) {
        num_storage_buffers = sjson_child_count(jstorage_bufs);
    }

    int total_sz = sizeof(rizz_shader_refl) + sizeof(rizz_shader_refl_input) * num_inputs +
                   sizeof(rizz_shader_refl_uniform_buffer) * num_uniforms +
                   sizeof(rizz_shader_refl_texture) * num_textures;

    rizz_shader_refl* refl = (rizz_shader_refl*)sx_malloc(alloc, total_sz);
    if (!refl) {
        sx_out_of_memory();
        return NULL;
    }
    sx_memset(refl, 0x0, sizeof(rizz_shader_refl));

    refl->lang = rizz__shader_str_to_lang(sjson_get_string(jroot, "language", ""));
    refl->stage = stage;
    refl->profile_version = sjson_get_int(jroot, "profile_version", 0);
    refl->code_type = sjson_get_bool(jroot, "bytecode", false) ? RIZZ_SHADER_CODE_BYTECODE
                                                               : RIZZ_SHADER_CODE_SOURCE;
    refl->flatten_ubos = sjson_get_bool(jroot, "flatten_ubos", false);
    sx_strcpy(refl->source_file, sizeof(refl->source_file), sjson_get_string(jstage, "file", ""));
    void* buff = refl + 1;
    if (jinputs) {
        refl->inputs = (rizz_shader_refl_input*)buff;
        sjson_node* jinput;
        rizz_shader_refl_input* input = refl->inputs;
        sjson_foreach(jinput, jinputs)
        {
            sx_strcpy(input->name, sizeof(input->name), sjson_get_string(jinput, "name", ""));
            sx_strcpy(input->semantic, sizeof(input->semantic),
                      sjson_get_string(jinput, "semantic", ""));
            input->semantic_index = sjson_get_int(jinput, "semantic_index", 0);
            input->type = rizz__shader_str_to_vertex_format(sjson_get_string(jinput, "type", ""));
            ++input;
        }
        refl->num_inputs = num_inputs;
        buff = input;
    }

    if (juniforms) {
        refl->uniform_buffers = (rizz_shader_refl_uniform_buffer*)buff;
        sjson_node* jubo;
        rizz_shader_refl_uniform_buffer* ubo = refl->uniform_buffers;
        sjson_foreach(jubo, juniforms)
        {
            sx_strcpy(ubo->name, sizeof(ubo->name), sjson_get_string(jubo, "name", ""));
            ubo->size_bytes = sjson_get_int(jubo, "block_size", 0);
            ubo->binding = sjson_get_int(jubo, "binding", 0);
            ubo->array_size = sjson_get_int(jubo, "array", 1);
            if (ubo->array_size > 1)
                sx_assert(refl->flatten_ubos &&
                          "arrayed uniform buffers should only be generated with --flatten-ubos");
            ++ubo;
        }
        refl->num_uniform_buffers = num_uniforms;
        buff = ubo;
    }

    if (jtextures) {
        refl->textures = (rizz_shader_refl_texture*)buff;
        sjson_node* jtex;
        rizz_shader_refl_texture* tex = refl->textures;
        sjson_foreach(jtex, jtextures)
        {
            sx_strcpy(tex->name, sizeof(tex->name), sjson_get_string(jtex, "name", ""));
            tex->binding = sjson_get_int(jtex, "binding", 0);
            tex->type = rizz__shader_str_to_texture_type(sjson_get_string(jtex, "dimension", ""),
                                                         sjson_get_bool(jtex, "array", false));
            ++tex;
        }
        refl->num_textures = num_textures;
        buff = tex;
    }

    if (jstorage_imgs) {
        refl->storage_images = (rizz_shader_refl_texture*)buff;
        sjson_node* jstorage_img;
        rizz_shader_refl_texture* img = refl->storage_images;
        sjson_foreach(jstorage_img, jstorage_imgs)
        {
            sx_strcpy(img->name, sizeof(img->name), sjson_get_string(jstorage_img, "name", ""));
            img->binding = sjson_get_int(jstorage_img, "binding", 0);
            img->type =
                rizz__shader_str_to_texture_type(sjson_get_string(jstorage_img, "dimension", ""),
                                                 sjson_get_bool(jstorage_img, "array", false));
            ++img;
        }
        refl->num_storage_images = num_storage_images;
        buff = img;
    }

    if (jstorage_bufs) {
        refl->storage_buffers = (rizz_shader_refl_buffer*)buff;
        sjson_node* jstorage_buf;
        rizz_shader_refl_buffer* sbuf = refl->storage_buffers;
        sjson_foreach(jstorage_buf, jstorage_bufs)
        {
            sx_strcpy(sbuf->name, sizeof(sbuf->name), sjson_get_string(jstorage_buf, "name", ""));
            sbuf->size_bytes = sjson_get_int(jstorage_buf, "block_size", 0);
            sbuf->binding = sjson_get_int(jstorage_buf, "binding", 0);
            sbuf->array_stride = sjson_get_int(jstorage_buf, "unsized_array_stride", 1);
            ++sbuf;
        }
        refl->num_uniform_buffers = num_uniforms;
        buff = sbuf;
    }

    if (close)
        sjson_destroy_context(jctx);

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
                                               const void* fs, int fs_size)
{
    sx_memset(desc, 0x0, sizeof(sg_shader_desc));
    const int num_stages = 2;
    rizz__shader_setup_desc_stage stages[] = {
        { .refl = vs_refl, .code = vs, .code_size = vs_size },
        { .refl = fs_refl, .code = fs, .code_size = fs_size }
    };

    for (int i = 0; i < num_stages; i++) {
        const rizz__shader_setup_desc_stage* stage = &stages[i];
        sg_shader_stage_desc* stage_desc = NULL;
        // clang-format off
        switch (stage->refl->stage) {
        case RIZZ_SHADER_STAGE_VS:   stage_desc = &desc->vs;             break;
        case RIZZ_SHADER_STAGE_FS:   stage_desc = &desc->fs;             break;
        default:                     sx_assert(0 && "not implemented");  break;
        }
        // clang-format on

        if (SX_PLATFORM_APPLE)
            stage_desc->entry = "main0";

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
                                                  int cs_size)
{
    sx_memset(desc, 0x0, sizeof(sg_shader_desc));
    const int num_stages = 1;
    rizz__shader_setup_desc_stage stages[] = {
        { .refl = cs_refl, .code = cs, .code_size = cs_size }
    };

    for (int i = 0; i < num_stages; i++) {
        const rizz__shader_setup_desc_stage* stage = &stages[i];
        sg_shader_stage_desc* stage_desc = NULL;
        // clang-format off
        switch (stage->refl->stage) {
        case RIZZ_SHADER_STAGE_CS:   stage_desc = &desc->cs;             break;
        default:                     sx_assert(0 && "not implemented");  break;
        }
        // clang-format on

        if (SX_PLATFORM_APPLE) {
            stage_desc->entry = "main0";
        }

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

    sjson_context* jctx = sjson_create_context(0, 0, (void*)alloc);
    if (!jctx) {
        return (rizz_shader){ { 0 } };
    }

    sg_shader_desc shader_desc = { 0 };
    rizz_shader_refl* vs_refl =
        rizz__shader_parse_reflect_json(alloc, (const char*)vs_refl_json, jctx);
    sjson_reset_context(jctx);
    rizz_shader_refl* fs_refl =
        rizz__shader_parse_reflect_json(alloc, (const char*)fs_refl_json, jctx);
    sjson_destroy_context(jctx);

    rizz_shader s = { .shd = the__gfx.make_shader(
                          rizz__shader_setup_desc(&shader_desc, vs_refl, vs_data, (int)vs_data_size,
                                                  fs_refl, fs_data, (int)fs_data_size)) };

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
            rizz_log_error("vertex attribute '%s%d' does not exist in actual shader inputs",
                           attr->semantic, attr->semantic_idx);
            sx_assert(0);
        }

        ++attr;
        ++index;
    }

    return desc;
}


static sg_pipeline_desc* rizz__shader_bindto_pipeline(rizz_shader* shd, sg_pipeline_desc* desc,
                                                      const rizz_vertex_layout* vl)
{
    return rizz__shader_bindto_pipeline_sg(shd->shd, shd->info.inputs, shd->info.num_inputs, desc,
                                           vl);
}

static rizz_asset_load_data rizz__shader_on_prepare(const rizz_asset_load_params* params,
                                                    const void* metadata)
{
    const sx_alloc* alloc = params->alloc ? params->alloc : g_gfx_alloc;
    const rizz_shader_info* info = metadata;

    rizz_shader* shader = sx_malloc(alloc, sizeof(rizz_shader));
    if (!shader)
        return (rizz_asset_load_data){ .obj = { 0 } };
    shader->shd = the__gfx.alloc_shader();
    sx_memcpy(&shader->info, info, sizeof(*info));

    return (rizz_asset_load_data){ .obj = { .ptr = shader },
                                   .user = sx_malloc(g_gfx_alloc, sizeof(sg_shader_desc)) };
}

static bool rizz__shader_on_load(rizz_asset_load_data* data, const rizz_asset_load_params* params,
                                 const sx_mem_block* mem)
{
    sx_unused(params);

    const sx_alloc* tmp_alloc = the__core.tmp_alloc_push();
    sg_shader_desc* shader_desc = data->user;

    rizz_shader_refl *vs_refl = NULL, *fs_refl = NULL, *cs_refl = NULL;
    const uint8_t *vs_data = NULL, *fs_data = NULL, *cs_data = NULL;
    int vs_size = 0, fs_size = 0, cs_size = 0;

    sx_mem_reader reader;
    sx_mem_init_reader(&reader, mem->data, mem->size);
    uint32_t _sgs;
    sx_mem_read_var(&reader, _sgs);
    if (_sgs != SGS_CHUNK) {
        return false;
    }
    sx_mem_seekr(&reader, sizeof(uint32_t), SX_WHENCE_CURRENT);

    struct sgs_chunk sinfo;
    sx_mem_read_var(&reader, sinfo);

    // read stages
    sx_iff_chunk stage_chunk = sx_mem_get_iff_chunk(&reader, 0, SGS_CHUNK_STAG);
    while (stage_chunk.pos != -1) {
        uint32_t stage_type;
        sx_mem_read_var(&reader, stage_type);

        rizz_shader_code_type code_type = RIZZ_SHADER_CODE_SOURCE;
        rizz_shader_stage stage;

        sx_iff_chunk code_chunk = sx_mem_get_iff_chunk(&reader, stage_chunk.size, SGS_CHUNK_CODE);
        if (code_chunk.pos == -1) {
            code_chunk = sx_mem_get_iff_chunk(&reader, stage_chunk.size, SGS_CHUNK_DATA);
            if (code_chunk.pos == -1)
                return false;    // nor data or code chunk is found!
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
            sx_assert(0 && "not implemented");
            stage = _RIZZ_SHADER_STAGE_COUNT;
        }

        // look for reflection chunk
        sx_mem_seekr(&reader, code_chunk.size, SX_WHENCE_CURRENT);
        sx_iff_chunk reflect_chunk =
            sx_mem_get_iff_chunk(&reader, stage_chunk.size - code_chunk.size, SGS_CHUNK_REFL);
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
        stage_chunk = sx_mem_get_iff_chunk(&reader, 0, SGS_CHUNK_STAG);
    }

    if (cs_refl && cs_data) {
        rizz__shader_setup_desc_cs(shader_desc, cs_refl, cs_data, cs_size);
    } else {
        sx_assert(vs_refl && fs_refl);
        rizz__shader_setup_desc(shader_desc, vs_refl, vs_data, vs_size, fs_refl, fs_data, fs_size);
    }

    the__core.tmp_alloc_pop();
    return true;
}

static void rizz__shader_on_finalize(rizz_asset_load_data* data,
                                     const rizz_asset_load_params* params, const sx_mem_block* mem)
{
    sx_unused(mem);
    sx_unused(params);

    rizz_shader* shader = data->obj.ptr;
    sg_shader_desc* desc = data->user;
    sx_assert(desc);

    the__gfx.init_shader(shader->shd, desc);

    sx_free(g_gfx_alloc, data->user);
}

static void rizz__shader_on_reload(rizz_asset handle, rizz_asset_obj prev_obj,
                                   const sx_alloc* alloc)
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
    sx_free(alloc, shader);
}

static void rizz__shader_on_read_metadata(void* metadata, const rizz_asset_load_params* params,
                                          const sx_mem_block* mem)
{
    sx_unused(params);

    rizz_shader_info* info = metadata;
    sx_memset(info, 0x0, sizeof(rizz_shader_info));

    sx_mem_reader reader;
    sx_mem_init_reader(&reader, mem->data, mem->size);

    uint32_t _sgs;
    sx_mem_read_var(&reader, _sgs);
    if (_sgs != SGS_CHUNK) {
        return;
    }
    sx_mem_seekr(&reader, sizeof(uint32_t), SX_WHENCE_CURRENT);

    struct sgs_chunk sinfo;
    sx_mem_read_var(&reader, sinfo);

    // read stages
    sx_iff_chunk stage_chunk = sx_mem_get_iff_chunk(&reader, 0, SGS_CHUNK_STAG);
    while (stage_chunk.pos != -1) {
        uint32_t stage_type;
        sx_mem_read_var(&reader, stage_type);

        if (stage_type == SGS_STAGE_VERTEX) {
            // look for reflection chunk
            sx_iff_chunk reflect_chunk =
                sx_mem_get_iff_chunk(&reader, stage_chunk.size, SGS_CHUNK_REFL);
            if (reflect_chunk.pos != -1) {
                const sx_alloc* tmp_alloc = the__core.tmp_alloc_push();
                rizz_shader_refl* refl = rizz__shader_parse_reflect_bin(
                    tmp_alloc, reader.data + reflect_chunk.pos, reflect_chunk.size);
                sx_memcpy(info->inputs, refl->inputs,
                          sizeof(rizz_shader_refl_input) * refl->num_inputs);
                info->num_inputs = refl->num_inputs;
                the__core.tmp_alloc_pop();
            }
        }

        sx_mem_seekr(&reader, stage_chunk.pos + stage_chunk.size, SX_WHENCE_BEGIN);
        stage_chunk = sx_mem_get_iff_chunk(&reader, 0, SGS_CHUNK_STAG);
    }
}

static void rizz__shader_init()
{
    // NOTE: shaders are always forced to load in blocking mode
    the__asset.register_asset_type(
        "shader",
        (rizz_asset_callbacks){ .on_prepare = rizz__shader_on_prepare,
                                .on_load = rizz__shader_on_load,
                                .on_finalize = rizz__shader_on_finalize,
                                .on_reload = rizz__shader_on_reload,
                                .on_release = rizz__shader_on_release,
                                .on_read_metadata = rizz__shader_on_read_metadata },
        NULL, 0, "rizz_shader_info", sizeof(rizz_shader_info), (rizz_asset_obj){ .ptr = NULL },
        (rizz_asset_obj){ .ptr = NULL }, RIZZ_ASSET_LOAD_FLAG_WAIT_ON_LOAD);
    rizz_refl_enum(sg_vertex_format, SG_VERTEXFORMAT_FLOAT);
    rizz_refl_enum(sg_vertex_format, SG_VERTEXFORMAT_FLOAT2);
    rizz_refl_enum(sg_vertex_format, SG_VERTEXFORMAT_FLOAT3);
    rizz_refl_enum(sg_vertex_format, SG_VERTEXFORMAT_FLOAT4);
    rizz_refl_enum(sg_vertex_format, SG_VERTEXFORMAT_BYTE4);
    rizz_refl_enum(sg_vertex_format, SG_VERTEXFORMAT_BYTE4N);
    rizz_refl_enum(sg_vertex_format, SG_VERTEXFORMAT_UBYTE4);
    rizz_refl_enum(sg_vertex_format, SG_VERTEXFORMAT_UBYTE4N);
    rizz_refl_enum(sg_vertex_format, SG_VERTEXFORMAT_SHORT2);
    rizz_refl_enum(sg_vertex_format, SG_VERTEXFORMAT_SHORT2N);
    rizz_refl_enum(sg_vertex_format, SG_VERTEXFORMAT_SHORT4);
    rizz_refl_enum(sg_vertex_format, SG_VERTEXFORMAT_SHORT4N);
    rizz_refl_enum(sg_vertex_format, SG_VERTEXFORMAT_UINT10_N2);

    rizz_refl_field(rizz_shader_refl_input, char[32], name, "shader input name");
    rizz_refl_field(rizz_shader_refl_input, char[32], semantic, "shader semantic name");
    rizz_refl_field(rizz_shader_refl_input, int, semantic_index, "shader semantic index");
    rizz_refl_field(rizz_shader_refl_input, sg_vertex_format, type, "shader input type");

    rizz_refl_field(rizz_shader_info, rizz_shader_refl_input[SG_MAX_VERTEX_ATTRIBUTES], inputs,
                    "shader inputs");
    rizz_refl_field(rizz_shader_info, int, num_inputs, "shader input count");
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// @font
typedef struct {
    uint32_t second_id;
    float amount;
} rizz__font_glyph_kern;

typedef struct {
    uint32_t id;
    sx_rect uvs;
    float xoffset;     // pixels to offset right/left
    float yoffset;     // pixels to offset up/down
    float xadvance;    // pixels to advance to next char
    int num_kerns;
    int kern_idx;
} rizz__font_glyph;

typedef struct {
    rizz_font f;
    rizz__font_glyph* glyphs;
    rizz__font_glyph_kern* kerns;
    sx_hashtbl glyph_tbl;    // key: glyph-id, value: index-to-glyphs
} rizz__font;

typedef struct {
    sx_vec2 pos;
    sx_vec2 uv;
    sx_vec3 transform1;
    sx_vec3 transform2;
    sx_color color;
} rizz__text_vertex;

// FNT binary format
#define RIZZ__FNT_SIGN "BMF"
#pragma pack(push, 1)
typedef struct {
    int16_t font_size;
    int8_t bit_field;
    uint8_t charset;
    uint16_t stretch_h;
    uint8_t aa;
    uint8_t padding_up;
    uint8_t padding_right;
    uint8_t padding_dwn;
    uint8_t padding_left;
    uint8_t spacing_horz;
    uint8_t spacing_vert;
    uint8_t outline;
    /* name (str) */
} rizz__fnt_info;

typedef struct {
    uint16_t line_height;
    uint16_t base;
    uint16_t scale_w;
    uint16_t scale_h;
    uint16_t pages;
    int8_t bit_field;
    uint8_t alpha_channel;
    uint8_t red_channel;
    uint8_t green_channel;
    uint8_t blue_channel;
} rizz__fnt_common;

typedef struct {
    char path[255];
} rizz__fnt_page;

typedef struct {
    uint32_t id;
    int16_t x;
    int16_t y;
    int16_t width;
    int16_t height;
    int16_t xoffset;
    int16_t yoffset;
    int16_t xadvance;
    uint8_t page;
    uint8_t channel;
} rizz__fnt_char;

typedef struct {
    uint32_t first;
    uint32_t second;
    int16_t amount;
} rizz__fnt_kern;

typedef struct {
    uint8_t id;
    uint32_t size;
} rizz__fnt_block;
#pragma pack(pop)

// metadata
typedef struct {
    char img_filepath[RIZZ_MAX_PATH];
    int img_width;
    int img_height;
    int num_glyphs;
    int num_kerns;
} rizz__font_metadata;

static char* rizz__fnt_text_read_keyval(char* token, char* key, char* value, int max_chars)
{
    char* equal = (char*)sx_strchar(token, '=');
    if (equal) {
        sx_strncpy(key, max_chars, token, (int)(intptr_t)(equal - token));

        char* val_start = (char*)sx_skip_whitespace(equal + 1);
        char* val_end;
        if (*val_start == '"') {
            ++val_start;
            val_end = *val_start != '"' ? (char*)sx_strchar(val_start, '"') : val_start;
        } else {
            // search for next whitespace (where key=value ends)
            val_end = val_start;
            while (!sx_isspace(*val_end))
                ++val_end;
        }
        sx_strncpy(value, max_chars, val_start, (int)(intptr_t)(val_end - val_start));
        return *val_end != '"' ? val_end : (val_end + 1);
    } else {
        key[0] = 0;
        value[0] = 0;
        return (char*)sx_skip_word(token);
    }
}

static int rizz__fnt_text_parse_numbers(char* value, int16_t* numbers, int max_numbers)
{
    int index = 0;
    char snum[16];
    char* token = (char*)sx_strchar(value, ',');
    while (token) {
        sx_strncpy(snum, sizeof(snum), value, (int)(intptr_t)(token - value));
        if (index < max_numbers)
            numbers[index++] = (int16_t)sx_toint(snum);
        value = token + 1;
        token = (char*)sx_strchar(value, ',');
    }
    sx_strcpy(snum, sizeof(snum), value);
    if (index < max_numbers)
        numbers[index++] = (int16_t)sx_toint(snum);
    return index;
}

static void rizz__fnt_text_read_info(char* token, rizz_font* font)
{
    char key[32], value[32];

    token = (char*)sx_skip_whitespace(token);
    while (*token) {
        token = rizz__fnt_text_read_keyval(token, key, value, 32);
        if (sx_strequal(key, "face")) {
            sx_strcpy(font->name, 32, value);
        } else if (sx_strequal(key, "size")) {
            font->size = sx_toint(value);
        } else if (sx_strequal(key, "bold")) {
            font->flags |= RIZZ_FONT_BOLD;
        } else if (sx_strequal(key, "italic")) {
            font->flags |= RIZZ_FONT_ITALIC;
        } else if (sx_strequal(key, "unicode") && sx_tobool(value)) {
            font->flags |= RIZZ_FONT_UNICODE;
        } else if (sx_strequal(key, "fixedHeight")) {
            font->flags |= RIZZ_FONT_FIXED_HEIGHT;
        } else if (sx_strequal(key, "padding")) {
            rizz__fnt_text_parse_numbers(value, font->padding, 4);
        } else if (sx_strequal(key, "spacing")) {
            rizz__fnt_text_parse_numbers(value, font->spacing, 2);
        }
        token = (char*)sx_skip_whitespace(token);
    }
}

static void rizz__fnt_text_read_common(char* token, rizz_font* font)
{
    char key[32], value[32];
    token = (char*)sx_skip_whitespace(token);
    while (*token) {
        token = rizz__fnt_text_read_keyval(token, key, value, 32);
        if (sx_strequal(key, "lineHeight")) {
            font->line_height = sx_toint(value);
        } else if (sx_strequal(key, "base")) {
            font->base = sx_toint(value);
        } else if (sx_strequal(key, "scaleW")) {
            font->img_width = sx_toint(value);
        } else if (sx_strequal(key, "scaleH")) {
            font->img_height = sx_toint(value);
        }
        token = (char*)sx_skip_whitespace(token);
    }
}

static void rizz__fnt_text_read_page(char* token, char* img_filepath, int img_filepath_sz,
                                     const char* fnt_path)
{
    char key[32], value[32];
    token = (char*)sx_skip_whitespace(token);
    while (*token) {
        token = rizz__fnt_text_read_keyval(token, key, value, 32);
        if (sx_strequal(key, "file")) {
            char dirname[RIZZ_MAX_PATH];
            sx_os_path_dirname(dirname, sizeof(dirname), fnt_path);
            sx_os_path_join(img_filepath, img_filepath_sz, dirname, value);
        }
        token = (char*)sx_skip_whitespace(token);
    }
}

static int rizz__fnt_text_read_count(char* token)
{
    char key[32], value[32];
    token = (char*)sx_skip_whitespace(token);
    token = rizz__fnt_text_read_keyval(token, key, value, 32);
    if (sx_strequal(key, "count"))
        return sx_toint(value);
    sx_assert(0 && "invalid file format");
    return 0;
}

static void rizz__fnt_text_read_char(char* token, rizz__font_glyph* g, int* char_width,
                                     float img_width, float img_height)
{
    char key[32], value[32];
    float x = 0, y = 0, w = 0, h = 0;
    token = (char*)sx_skip_whitespace(token);
    while (*token) {
        token = rizz__fnt_text_read_keyval(token, key, value, 32);
        if (strcmp(key, "id") == 0) {
            g->id = sx_touint(value);
        } else if (strcmp(key, "x") == 0) {
            x = (float)sx_toint(value);
        } else if (strcmp(key, "y") == 0) {
            y = (float)sx_toint(value);
        } else if (strcmp(key, "width") == 0) {
            w = (float)sx_toint(value);
        } else if (strcmp(key, "height") == 0) {
            h = (float)sx_toint(value);
        } else if (strcmp(key, "xoffset") == 0) {
            g->xoffset = (float)sx_toint(value);
        } else if (strcmp(key, "yoffset") == 0) {
            g->yoffset = (float)sx_toint(value);
        } else if (strcmp(key, "xadvance") == 0) {
            int xadvance = sx_toint(value);
            g->xadvance = (float)xadvance;
            *char_width = sx_max(*char_width, xadvance);
        }
        token = (char*)sx_skip_whitespace(token);
    }

    g->uvs = sx_rectwh(x / img_width, y / img_height, w / img_width, h / img_height);
}

static void rizz__fnt_text_read_kern(char* token, rizz__font_glyph_kern* k, int kern_idx,
                                     rizz__font_glyph* glyphs, int num_glyphs)
{
    char key[32], value[32];
    int first_glyph_idx = -1;
    token = (char*)sx_skip_whitespace(token);
    while (*token) {
        token = rizz__fnt_text_read_keyval(token, key, value, 32);
        if (strcmp(key, "first") == 0) {
            uint32_t first_char_id = sx_touint(value);
            for (int i = 0; i < num_glyphs; i++) {
                if (glyphs[i].id != first_char_id)
                    continue;
                first_glyph_idx = i;
                break;
            }
        } else if (strcmp(key, "second") == 0) {
            k->second_id = sx_touint(value);
        } else if (strcmp(key, "amount") == 0) {
            k->amount = (float)sx_toint(value);
        }
        token = (char*)sx_skip_whitespace(token);
    }

    sx_assert(first_glyph_idx != -1);
    if (glyphs[first_glyph_idx].num_kerns == 0)
        glyphs[first_glyph_idx].kern_idx = kern_idx;
    ++glyphs[first_glyph_idx].num_kerns;
}

static bool rizz__fnt_text_check(char* buff, int len)
{
    for (int i = 0; i < len; i++) {
        if (buff[i] <= 0)
            return false;
    }
    return true;
}

static rizz_asset_load_data rizz__font_on_prepare(const rizz_asset_load_params* params,
                                                  const void* metadata)
{
    const sx_alloc* alloc = params->alloc ? params->alloc : g_gfx_alloc;
    const rizz__font_metadata* meta = metadata;
    sx_assert(meta);

    int hashtbl_cap = sx_hashtbl_valid_capacity(meta->num_glyphs);
    int total_sz = sizeof(rizz__font) + meta->num_glyphs * sizeof(rizz__font_glyph) +
                   meta->num_kerns * sizeof(rizz__font_glyph_kern) +
                   hashtbl_cap * (sizeof(uint32_t) + sizeof(int));
    rizz__font* font = sx_malloc(alloc, total_sz);
    if (!font) {
        sx_out_of_memory();
        return (rizz_asset_load_data){ .obj = { 0 } };
    }
    sx_memset(font, 0x0, total_sz);

    // initialize font buffers/hashtbl and texture
    uint8_t* buff = (uint8_t*)(font + 1);
    font->glyphs = (rizz__font_glyph*)buff;
    buff += sizeof(rizz__font_glyph) * meta->num_glyphs;
    font->kerns = (rizz__font_glyph_kern*)buff;
    buff += sizeof(rizz__font_glyph_kern) * meta->num_kerns;
    uint32_t* keys = (uint32_t*)buff;
    buff += sizeof(uint32_t) * hashtbl_cap;
    int* values = (int*)buff;

    sx_hashtbl_init(&font->glyph_tbl, hashtbl_cap, keys, values);

    rizz_texture_load_params tparams = { 0 };
    font->f.img = the__asset.load("texture", meta->img_filepath, &tparams, params->flags, alloc,
                                  params->tags);

    return (rizz_asset_load_data){ .obj = { .ptr = font } };
}

static bool rizz__font_on_load(rizz_asset_load_data* data, const rizz_asset_load_params* params,
                               const sx_mem_block* mem)
{
    const sx_alloc* tmp_alloc = the__core.tmp_alloc_push();

    rizz__font* font = data->obj.ptr;
    sx_mem_reader rd;
    sx_mem_init_reader(&rd, mem->data, mem->size);

    // read first 3-bytes and check if we have binary or (probably) text formats
    char sign_str[4];
    sx_mem_read(&rd, sign_str, 3);
    sign_str[3] = '\0';
    if (sx_strequal(sign_str, RIZZ__FNT_SIGN)) {
        // Load binary
        uint8_t file_ver;
        sx_mem_read(&rd, &file_ver, sizeof(file_ver));
        if (file_ver != 3) {
            rizz_log_warn("loading font '%s' failed: invalid file version", params->path);
            return false;
        }

        //
        rizz__fnt_block block;
        rizz__fnt_info info;
        rizz__fnt_common common;
        sx_mem_read(&rd, &block, sizeof(block));
        sx_assert(block.id == 1);
        sx_mem_read(&rd, &info, sizeof(info));
        sx_mem_read(&rd, font->f.name,
                    (int)sx_min(sizeof(font->f.name), block.size - sizeof(info)));

        // Common
        sx_mem_read(&rd, &block, sizeof(block));
        sx_assert(block.id == 2);
        sx_mem_read(&rd, &common, sizeof(common));

        if (common.pages > 1) {
            rizz_log_warn("loading font '%s' failed: should contain only 1 page", params->path);
            return false;
        }

        // Font pages (textures)
        rizz__fnt_page page;
        char dirname[RIZZ_MAX_PATH];
        char img_filepath[RIZZ_MAX_PATH];
        sx_os_path_dirname(dirname, sizeof(dirname), params->path);

        sx_mem_read(&rd, &block, sizeof(block));
        sx_assert(block.id == 3);
        sx_mem_read(&rd, page.path, block.size);
        sx_os_path_join(img_filepath, sizeof(img_filepath), dirname, page.path);

        // Characters (glyphs)
        sx_mem_read(&rd, &block, sizeof(block));
        sx_assert(block.id == 4);
        int num_glyphs = block.size / sizeof(rizz__fnt_char);
        rizz__fnt_char* chars = sx_malloc(tmp_alloc, sizeof(rizz__fnt_char) * num_glyphs);
        sx_assert(chars);
        sx_mem_read(&rd, chars, block.size);

        // Kernings
        int last_r = rd.pos != rd.top ? sx_mem_read(&rd, &block, sizeof(block)) : 0;
        int num_kerns = block.size / sizeof(rizz__fnt_kern);
        rizz__fnt_kern* kerns = NULL;
        if (block.id == 5 && num_kerns > 0 && last_r == sizeof(block)) {
            kerns = sx_malloc(tmp_alloc, sizeof(rizz__fnt_kern) * num_kerns);
            sx_assert(kerns);
            sx_mem_read(&rd, kerns, block.size);
        }

        // Create font
        font->f.size = info.font_size;
        font->f.line_height = common.line_height;
        font->f.base = common.base;
        font->f.img_width = common.scale_w;
        font->f.img_height = common.scale_h;
        font->f.num_glyphs = num_glyphs;
        font->f.num_kerns = num_kerns;

        float img_width = (float)common.scale_w;
        float img_height = (float)common.scale_h;

        // image is already loaded in on_prepare
        // glyph_tbl is already initialized in on_prepare
        int16_t char_width = 0;
        for (int i = 0; i < num_glyphs; i++) {
            const rizz__fnt_char* ch = &chars[i];
            font->glyphs[i].id = ch->id;
            font->glyphs[i].uvs =
                sx_rectwh((float)ch->x / img_width, (float)ch->y / img_height,
                          (float)ch->width / img_width, (float)ch->height / img_height);
            font->glyphs[i].xadvance = (float)ch->xadvance;
            font->glyphs[i].xoffset = (float)ch->xoffset;
            font->glyphs[i].yoffset = (float)ch->yoffset;

            sx_hashtbl_add(&font->glyph_tbl, ch->id, i);

            char_width = sx_max(char_width, ch->xadvance);
        }
        font->f.char_width = char_width;

        if (num_kerns > 0) {
            for (int i = 0; i < num_kerns; i++) {
                const rizz__fnt_kern* kern = &kerns[i];
                // Find char and set it's kerning index
                uint32_t id = kern->first;
                for (int k = 0; k < num_glyphs; k++) {
                    if (id == font->glyphs[k].id) {
                        if (font->glyphs[k].num_kerns == 0)
                            font->glyphs[k].kern_idx = i;
                        ++font->glyphs[k].num_kerns;
                        break;
                    }
                }

                font->kerns[i].second_id = kern->second;
                font->kerns[i].amount = (float)kern->amount;
            }
        }
    } else {
        // text
        // read line by line and parse needed information
        char* buff = sx_malloc(tmp_alloc, (size_t)mem->size + 1);
        if (!buff) {
            the__core.tmp_alloc_pop();
            sx_out_of_memory();
            return false;
        }
        sx_memcpy(buff, mem->data, mem->size);
        buff[mem->size] = '\0';

        if (!rizz__fnt_text_check(buff, mem->size)) {
            rizz_log_warn("loading font '%s' failed: not a valid fnt text file", params->path);
            the__core.tmp_alloc_pop();
            return false;
        }

        // check if it's character
        char* eol;
        char img_filepath[RIZZ_MAX_PATH];
        char line[1024];
        int char_idx = 0;
        int kern_idx = 0;

        while ((eol = (char*)sx_strchar(buff, '\n'))) {
            sx_strncpy(line, sizeof(line), buff, (int)(intptr_t)(eol - buff));
            char name[32];
            char* name_end = (char*)sx_skip_word(line);
            sx_assert(name_end);
            sx_strncpy(name, sizeof(name), line, (int)(intptr_t)(name_end - line));
            if (sx_strequal(name, "info")) {
                rizz__fnt_text_read_info(name_end, &font->f);
            } else if (sx_strequal(name, "common")) {
                rizz__fnt_text_read_common(name_end, &font->f);
            } else if (sx_strequal(name, "chars")) {
                font->f.num_glyphs = rizz__fnt_text_read_count(name_end);
            } else if (sx_strequal(name, "kernings")) {
                font->f.num_kerns = rizz__fnt_text_read_count(name_end);
            } else if (sx_strequal(name, "page")) {
                rizz__fnt_text_read_page(name_end, img_filepath, sizeof(img_filepath),
                                         params->path);
            } else if (sx_strequal(name, "char")) {
                // image is already loaded in on_prepare
                // glyph_tbl is already initialized in on_prepare
                rizz__fnt_text_read_char(name_end, &font->glyphs[char_idx], &font->f.char_width,
                                         (float)font->f.img_width, (float)font->f.img_height);
                sx_hashtbl_add(&font->glyph_tbl, font->glyphs[char_idx].id, char_idx);
                char_idx++;
            } else if (sx_strequal(name, "kerning")) {
                rizz__fnt_text_read_kern(name_end, &font->kerns[kern_idx], kern_idx, font->glyphs,
                                         font->f.num_glyphs);
                kern_idx++;
            }

            buff = (char*)sx_skip_whitespace(eol);
        }
    }    // if (binary)

    the__core.tmp_alloc_pop();
    return true;
}

static void rizz__font_on_finalize(rizz_asset_load_data* data, const rizz_asset_load_params* params,
                                   const sx_mem_block* mem)
{
    sx_unused(data);
    sx_unused(params);
    sx_unused(mem);
}

static void rizz__font_on_reload(rizz_asset handle, rizz_asset_obj prev_obj, const sx_alloc* alloc)
{
    sx_unused(handle);
    sx_unused(prev_obj);
    sx_unused(alloc);
}

static void rizz__font_on_release(rizz_asset_obj obj, const sx_alloc* alloc)
{
    rizz__font* font = obj.ptr;
    sx_assert(font);

    if (!alloc)
        alloc = g_gfx_alloc;

    if (font->f.img.id)
        the__asset.unload(font->f.img);

    sx_free(alloc, font);
}

static void rizz__font_on_read_metadata(void* metadata, const rizz_asset_load_params* params,
                                        const sx_mem_block* mem)
{
    rizz__font_metadata* meta = metadata;
    sx_mem_reader rd;
    sx_mem_init_reader(&rd, mem->data, mem->size);

    // read first 3-bytes and check if we have binary or (probably) text formats
    char sign_str[4];
    sx_mem_read(&rd, sign_str, 3);
    sign_str[3] = '\0';
    if (sx_strequal(sign_str, RIZZ__FNT_SIGN)) {
        uint8_t file_ver;
        sx_mem_read(&rd, &file_ver, sizeof(file_ver));
        if (file_ver != 3) {
            rizz_log_warn("loading font '%s' failed: invalid file version", params->path);
            return;
        }

        rizz__fnt_block block;
        rizz__fnt_info info;
        rizz__fnt_common common;
        char name[256];

        sx_mem_read(&rd, &block, sizeof(block));
        sx_assert(block.id == 1);
        sx_mem_read(&rd, &info, sizeof(info));
        sx_mem_read(&rd, name, block.size - sizeof(info));

        sx_mem_read(&rd, &block, sizeof(block));
        sx_assert(block.id == 2);
        sx_mem_read(&rd, &common, sizeof(common));
        if (common.pages > 1) {
            rizz_log_warn("loading font '%s' failed: should contain only 1 page", params->path);
            return;
        }

        // Font pages (textures)
        rizz__fnt_page page;
        char dirname[RIZZ_MAX_PATH];
        sx_os_path_dirname(dirname, sizeof(dirname), params->path);
        sx_mem_read(&rd, &block, sizeof(block));
        sx_assert(block.id == 3);
        sx_mem_read(&rd, page.path, block.size);
        sx_os_path_join(meta->img_filepath, sizeof(meta->img_filepath), dirname, page.path);
        sx_os_path_unixpath(meta->img_filepath, sizeof(meta->img_filepath), meta->img_filepath);

        meta->img_height = common.scale_w;
        meta->img_height = common.scale_h;

        sx_mem_read(&rd, &block, sizeof(block));
        sx_assert(block.id == 4);
        sx_mem_seekr(&rd, block.size, SX_WHENCE_CURRENT);
        meta->num_glyphs = block.size / sizeof(rizz__fnt_char);

        // Kernings
        int r = rd.pos != rd.top ? sx_mem_read(&rd, &block, sizeof(block)) : 0;
        if (block.id == 5 && r == sizeof(block))
            meta->num_kerns = block.size / sizeof(rizz__fnt_kern);
    } else {
        // text
        // read line by line and parse needed information
        const sx_alloc* tmp_alloc = the__core.tmp_alloc_push();
        char* buff = sx_malloc(tmp_alloc, (size_t)mem->size + 1);
        if (!buff) {
            sx_out_of_memory();
            return;
        }
        sx_memcpy(buff, mem->data, mem->size);
        buff[mem->size] = '\0';

        if (!rizz__fnt_text_check(buff, mem->size)) {
            rizz_log_warn("loading font '%s' failed: not a valid fnt text file", params->path);
            return;
        }

        // check if it's character
        char* eol;
        rizz_font fnt = { .name = { 0 } };
        char img_filepath[RIZZ_MAX_PATH];
        char line[1024];
        while ((eol = (char*)sx_strchar(buff, '\n'))) {
            sx_strncpy(line, sizeof(line), buff, (int)(intptr_t)(eol - buff));
            char name[32];
            char* name_end = (char*)sx_skip_word(line);
            sx_assert(name_end);
            sx_strncpy(name, sizeof(name), line, (int)(intptr_t)(name_end - line));
            if (sx_strequal(name, "info"))
                rizz__fnt_text_read_info(name_end, &fnt);
            else if (sx_strequal(name, "common"))
                rizz__fnt_text_read_common(name_end, &fnt);
            else if (sx_strequal(name, "chars"))
                fnt.num_glyphs = rizz__fnt_text_read_count(name_end);
            else if (sx_strequal(name, "kernings"))
                fnt.num_kerns = rizz__fnt_text_read_count(name_end);
            else if (sx_strequal(name, "page"))
                rizz__fnt_text_read_page(name_end, img_filepath, sizeof(img_filepath),
                                         params->path);

            buff = (char*)sx_skip_whitespace(eol);
        }

        sx_strcpy(meta->img_filepath, sizeof(meta->img_filepath), img_filepath);
        meta->img_width = fnt.img_width;
        meta->img_height = fnt.img_height;
        meta->num_glyphs = fnt.num_glyphs;
        meta->num_kerns = fnt.num_kerns;

        the__core.tmp_alloc_pop();
    }
}

static void rizz__font_init()
{
    rizz_refl_field(rizz__font_metadata, char[RIZZ_MAX_PATH], img_filepath, "img_filepath");
    rizz_refl_field(rizz__font_metadata, int, img_width, "img_width");
    rizz_refl_field(rizz__font_metadata, int, img_height, "img_height");
    rizz_refl_field(rizz__font_metadata, int, num_glyphs, "num_glyphs");
    rizz_refl_field(rizz__font_metadata, int, num_kerns, "num_kerns");

    // TODO: create default async/fail objects

    the__asset.register_asset_type(
        "font",
        (rizz_asset_callbacks){ .on_prepare = rizz__font_on_prepare,
                                .on_load = rizz__font_on_load,
                                .on_finalize = rizz__font_on_finalize,
                                .on_reload = rizz__font_on_reload,
                                .on_release = rizz__font_on_release,
                                .on_read_metadata = rizz__font_on_read_metadata },
        NULL, 0, "rizz__font_metadata", sizeof(rizz__font_metadata),
        (rizz_asset_obj){ .ptr = NULL }, (rizz_asset_obj){ .ptr = NULL }, 0);
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

    if (desc->render_target && _sg_is_valid_rendertarget_color_format(desc->pixel_format) &&
        _sg_is_valid_rendertarget_depth_format(desc->pixel_format)) {
        sx_assert(desc->num_mipmaps == 1);

        int bytesize = _sg_is_valid_rendertarget_depth_format(desc->pixel_format)
                           ? 4
                           : _sg_pixelformat_bytesize(desc->pixel_format);
        int pixels = desc->width * desc->height * desc->layers;
        int64_t size = (int64_t)pixels * bytesize;
        g_gfx.trace.t.render_target_size += size;
        g_gfx.trace.t.render_target_peak =
            sx_max(g_gfx.trace.t.render_target_peak, g_gfx.trace.t.render_target_size);
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

static void rizz__trace_make_pipeline(const sg_pipeline_desc* desc, sg_pipeline result,
                                      void* user_data)
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
    g_gfx.trace.t.buffer_size -= buf->size;
    --g_gfx.trace.t.num_buffers;
}

static void rizz__trace_destroy_image(sg_image img_id, void* user_data)
{
    sx_unused(user_data);
    _sg_image_t* img = _sg_lookup_image(&_sg.pools, img_id.id);
    if (img->render_target && _sg_is_valid_rendertarget_color_format(img->pixel_format) &&
        _sg_is_valid_rendertarget_depth_format(img->pixel_format)) {
        sx_assert(img->num_mipmaps == 1);

        int bytesize = _sg_is_valid_rendertarget_depth_format(img->pixel_format)
                           ? 4
                           : _sg_pixelformat_bytesize(img->pixel_format);
        int pixels = img->width * img->height * img->depth;
        int64_t size = (int64_t)pixels * bytesize;
        g_gfx.trace.t.render_target_size -= size;
    }
    --g_gfx.trace.t.num_images;
}

static void rizz__trace_destroy_shader(sg_shader shd, void* user_data)
{
    sx_unused(shd);
    sx_unused(user_data);
    --g_gfx.trace.t.num_shaders;
}

static void rizz__trace_destroy_pipeline(sg_pipeline pip, void* user_data)
{
    sx_unused(pip);
    sx_unused(user_data);
    --g_gfx.trace.t.num_pipelines;
}

static void rizz__trace_destroy_pass(sg_pass pass, void* user_data)
{
    sx_unused(pass);
    sx_unused(user_data);
    --g_gfx.trace.t.num_passes;
}

static void rizz__trace_begin_pass(sg_pass pass, const sg_pass_action* pass_action, void* user_data)
{
    sx_unused(user_data);
    sx_unused(pass);
    sx_unused(pass_action);
    ++g_gfx.trace.t.num_apply_passes;
}

static void rizz__trace_begin_default_pass(const sg_pass_action* pass_action, int width, int height,
                                           void* user_data)
{
    sx_unused(pass_action);
    sx_unused(width);
    sx_unused(height);
    sx_unused(user_data);
    ++g_gfx.trace.t.num_apply_passes;
}

static void rizz__trace_apply_pipeline(sg_pipeline pip, void* user_data)
{
    sx_unused(user_data);
    sx_unused(pip);
    ++g_gfx.trace.t.num_apply_pipelines;
}

static void rizz__trace_draw(int base_element, int num_elements, int num_instances, void* user_data)
{
    sx_unused(user_data);
    sx_unused(base_element);

    ++g_gfx.trace.t.num_draws;
    g_gfx.trace.t.num_instances += num_instances;
    g_gfx.trace.t.num_elements += num_elements;
}

void rizz__gfx_trace_reset_frame_stats()
{
    g_gfx.trace.t.num_draws = 0;
    g_gfx.trace.t.num_instances = 0;
    g_gfx.trace.t.num_elements = 0;
    g_gfx.trace.t.num_apply_pipelines = 0;
    g_gfx.trace.t.num_apply_passes = 0;
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
        if (frame > buf->used_frame + 1) {
            if (buf->usage == SG_USAGE_STREAM) {
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

    // shaders
    for (int i = 0, c = sx_array_count(g_gfx.destroy_shaders); i < c; i++) {
        _sg_pipeline_t* shd = _sg_lookup_pipeline(&_sg.pools, g_gfx.destroy_shaders[i].id);
        if (frame > shd->used_frame + 1) {
            sg_destroy_shader(g_gfx.destroy_shaders[i]);
            sx_array_pop(g_gfx.destroy_shaders, i);
            i--;
            c--;
        }
    }

    // pipelines
    for (int i = 0, c = sx_array_count(g_gfx.destroy_pips); i < c; i++) {
        sg_pipeline pip_id = g_gfx.destroy_pips[i];
        _sg_pipeline_t* pip = _sg_lookup_pipeline(&_sg.pools, pip_id.id);
        if (frame > pip->used_frame + 1) {
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

    // passes
    for (int i = 0, c = sx_array_count(g_gfx.destroy_passes); i < c; i++) {
        _sg_pass_t* pass = _sg_lookup_pass(&_sg.pools, g_gfx.destroy_passes[i].id);
        if (frame > pass->used_frame + 1) {
            sg_destroy_pass(g_gfx.destroy_passes[i]);
            sx_array_pop(g_gfx.destroy_passes, i);
            i--;
            c--;
        }
    }

    // images
    for (int i = 0, c = sx_array_count(g_gfx.destroy_images); i < c; i++) {
        _sg_image_t* img = _sg_lookup_image(&_sg.pools, g_gfx.destroy_images[i].id);
        if (frame > img->used_frame + 1) {
            sg_destroy_image(g_gfx.destroy_images[i]);
            sx_array_pop(g_gfx.destroy_images, i);
            i--;
            c--;
        }
    }
}

//
bool rizz__gfx_init(const sx_alloc* alloc, const sg_desc* desc, bool enable_profile)
{
#if SX_PLATFORM_LINUX
    if (flextInit() != GL_TRUE) {
        rizz_log_error("gfx: could not initialize OpenGL");
        return false;
    }
#endif
    g_gfx_alloc = alloc;
    sg_setup(desc);
    g_gfx.enable_profile = enable_profile;

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
    rizz__font_init();

    // profiler
    if (enable_profile) {
        if (RMT_USE_D3D11) {
            rmt_BindD3D11((void*)rizz__app_d3d11_device(), (void*)rizz__app_d3d11_device_context());
        } else if (RMT_USE_OPENGL) {
            rmt_BindOpenGL();
        }
    }

    // debug draw
    {
        g_gfx.dbg.vb = the__gfx.make_buffer(&(sg_buffer_desc){
            .type = SG_BUFFERTYPE_VERTEXBUFFER,
            .usage = SG_USAGE_STREAM,
            .size = sizeof(rizz__debug_vertex) * RIZZ_CONFIG_MAX_DEBUG_VERTICES });
        g_gfx.dbg.ib = the__gfx.make_buffer(&(sg_buffer_desc){
            .type = SG_BUFFERTYPE_INDEXBUFFER,
            .usage = SG_USAGE_STREAM,
            .size = sizeof(rizz__debug_vertex) * RIZZ_CONFIG_MAX_DEBUG_INDICES });

        const sx_alloc* tmp_alloc = the__core.tmp_alloc_push();
        rizz_shader shader = the__gfx.shader_make_with_data(
            tmp_alloc, k_debug_vs_size, k_debug_vs_data, k_debug_vs_refl_size, k_debug_vs_refl_data,
            k_debug_fs_size, k_debug_fs_data, k_debug_fs_refl_size, k_debug_fs_refl_data);

        g_gfx.dbg.shader = shader.shd;

        sg_pipeline_desc pip_desc_wire = { .layout.buffers[0].stride = sizeof(rizz__debug_vertex),
                                           .shader = g_gfx.dbg.shader,
                                           .index_type = SG_INDEXTYPE_NONE,
                                           .rasterizer = { .sample_count = 4 },
                                           .primitive_type = SG_PRIMITIVETYPE_LINES,
                                           .depth_stencil = { .depth_compare_func =
                                                                  SG_COMPAREFUNC_LESS_EQUAL } };
        the__gfx.shader_bindto_pipeline(&shader, &pip_desc_wire, &k__debug_vertex);

        g_gfx.dbg.pip_wire = the__gfx.make_pipeline(&pip_desc_wire);
        the__core.tmp_alloc_pop();
    }

    return true;
}

void rizz__gfx_release()
{
    // debug
    if (g_gfx.dbg.pip_wire.id)
        the__gfx.destroy_pipeline(g_gfx.dbg.pip_wire);
    if (g_gfx.dbg.shader.id)
        the__gfx.destroy_shader(g_gfx.dbg.shader);
    if (g_gfx.dbg.vb.id)
        the__gfx.destroy_buffer(g_gfx.dbg.vb);
    if (g_gfx.dbg.ib.id)
        the__gfx.destroy_buffer(g_gfx.dbg.ib);

    rizz__texture_release();

    // deferred destroys
    rizz__gfx_collect_garbage(the__core.frame_index() + 100);

    sx_array_free(g_gfx_alloc, g_gfx.destroy_buffers);
    sx_array_free(g_gfx_alloc, g_gfx.destroy_images);
    sx_array_free(g_gfx_alloc, g_gfx.destroy_passes);
    sx_array_free(g_gfx_alloc, g_gfx.destroy_pips);
    sx_array_free(g_gfx_alloc, g_gfx.destroy_shaders);

    for (int i = 0; i < sx_array_count(g_gfx.cmd_buffers); i++) {
        sx_assert(g_gfx.cmd_buffers[i]);
        rizz__gfx_cmdbuffer* cb = g_gfx.cmd_buffers[i];
        sx_assert(cb->running_stage.id == 0);
        sx_array_free(cb->alloc, cb->params_buff);
        sx_array_free(cb->alloc, cb->refs);
        sx_free(cb->alloc, cb);
    }
    sx_array_free(g_gfx_alloc, g_gfx.cmd_buffers);
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

    sg_shutdown();
}

void rizz__gfx_update()
{
    rizz__gfx_collect_garbage(the__core.frame_index());
}

void rizz__gfx_commit()
{
    sg_commit();
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

static inline uint8_t* rizz__cb_alloc_params_buff(rizz__gfx_cmdbuffer* cb, int size, int* offset)
{
    uint8_t* ptr = sx_array_add(cb->alloc, cb->params_buff,
                                sx_align_mask(size, SX_CONFIG_ALLOCATOR_NATURAL_ALIGNMENT - 1));
    if (!ptr) {
        sx_out_of_memory();
        return NULL;
    }
    *offset = (int)(intptr_t)(ptr - cb->params_buff);
    return ptr;
}

static void rizz__cb_record_begin_stage(const char* name, int name_sz)
{
    sx_assert(name_sz == 32);

    rizz__gfx_cmdbuffer* cb = (rizz__gfx_cmdbuffer*)rizz__core_gfx_cmdbuffer();

    sx_assert(cb->running_stage.id &&
              "draw related calls must come between begin_stage..end_stage");
    sx_assert(cb->cmd_idx < UINT16_MAX);

    int offset;
    uint8_t* buff = rizz__cb_alloc_params_buff(cb, name_sz, &offset);

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
    const char* name = (const char*)buff;
    buff += 32;    // TODO: match this with stage::name

    sg_push_debug_group(name);
    return buff;
}

static void rizz__cb_record_end_stage()
{
    rizz__gfx_cmdbuffer* cb = (rizz__gfx_cmdbuffer*)rizz__core_gfx_cmdbuffer();

    sx_assert(cb->running_stage.id &&
              "draw related calls must come between begin_stage..end_stage");
    sx_assert(cb->cmd_idx < UINT16_MAX);

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
    sg_pop_debug_group();
    return buff;
}

static bool rizz__cb_begin_stage(rizz_gfx_stage stage)
{
    rizz__gfx_cmdbuffer* cb = (rizz__gfx_cmdbuffer*)rizz__core_gfx_cmdbuffer();

    sx_lock(&g_gfx.stage_lk, 1);
    rizz__gfx_stage* _stage = &g_gfx.stages[rizz_to_index(stage.id)];
    sx_assert(_stage->state == STAGE_STATE_NONE && "already called begin on this stage");
    bool enabled = _stage->enabled;
    if (!enabled) {
        sx_unlock(&g_gfx.stage_lk);
        return false;
    }
    _stage->state = STAGE_STATE_SUBMITTING;
    cb->running_stage = stage;
    cb->stage_order = _stage->order;
    sx_unlock(&g_gfx.stage_lk);

    rizz__cb_record_begin_stage(_stage->name, sizeof(_stage->name));

    return true;
}

static void rizz__cb_end_stage()
{
    rizz__gfx_cmdbuffer* cb = (rizz__gfx_cmdbuffer*)rizz__core_gfx_cmdbuffer();
    sx_assert(cb->running_stage.id && "must call begin_stage before this call");

    sx_lock(&g_gfx.stage_lk, 1);
    rizz__gfx_stage* _stage = &g_gfx.stages[rizz_to_index(cb->running_stage.id)];
    sx_assert(_stage->state == STAGE_STATE_SUBMITTING && "should call begin on this stage first");
    _stage->state = STAGE_STATE_DONE;
    sx_unlock(&g_gfx.stage_lk);

    rizz__cb_record_end_stage();
    cb->running_stage = (rizz_gfx_stage){ 0 };
}


static void rizz__cb_begin_default_pass(const sg_pass_action* pass_action, int width, int height)
{
    rizz__gfx_cmdbuffer* cb = (rizz__gfx_cmdbuffer*)rizz__core_gfx_cmdbuffer();

    sx_assert(cb->running_stage.id &&
              "draw related calls must come between begin_stage..end_stage");
    sx_assert(cb->cmd_idx < UINT16_MAX);

    int offset;
    uint8_t* buff =
        rizz__cb_alloc_params_buff(cb, sizeof(sg_pass_action) + sizeof(int) * 2, &offset);
    sx_assert(buff);

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

static uint8_t* rizz__cb_run_begin_default_pass(uint8_t* buff)
{
    sg_pass_action* pass_action = (sg_pass_action*)buff;
    buff += sizeof(sg_pass_action);
    int width = *((int*)buff);
    buff += sizeof(int);
    int height = *((int*)buff);
    buff += sizeof(int);
    sg_begin_default_pass(pass_action, width, height);
    return buff;
}

static void rizz__cb_begin_pass(sg_pass pass, const sg_pass_action* pass_action)
{
    rizz__gfx_cmdbuffer* cb = (rizz__gfx_cmdbuffer*)rizz__core_gfx_cmdbuffer();

    sx_assert(cb->running_stage.id &&
              "draw related calls must come between begin_stage..end_stage");
    sx_assert(cb->cmd_idx < UINT16_MAX);

    int offset;
    uint8_t* buff =
        rizz__cb_alloc_params_buff(cb, sizeof(sg_pass_action) + sizeof(sg_pass), &offset);
    sx_assert(buff);

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
    _pass->used_frame = the__core.frame_index();
}

static uint8_t* rizz__cb_run_begin_pass(uint8_t* buff)
{
    sg_pass_action* pass_action = (sg_pass_action*)buff;
    buff += sizeof(sg_pass_action);
    sg_pass pass = *((sg_pass*)buff);
    buff += sizeof(sg_pass);
    sg_begin_pass(pass, pass_action);
    return buff;
}

static void rizz__cb_apply_viewport(int x, int y, int width, int height, bool origin_top_left)
{
    rizz__gfx_cmdbuffer* cb = (rizz__gfx_cmdbuffer*)rizz__core_gfx_cmdbuffer();

    sx_assert(cb->running_stage.id &&
              "draw related calls must come between begin_stage..end_stage");
    sx_assert(cb->cmd_idx < UINT16_MAX);

    int offset;
    uint8_t* buff = rizz__cb_alloc_params_buff(cb, sizeof(int) * 4 + sizeof(bool), &offset);
    sx_assert(buff);

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

static uint8_t* rizz__cb_run_apply_viewport(uint8_t* buff)
{
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

static void rizz__cb_apply_scissor_rect(int x, int y, int width, int height, bool origin_top_left)
{
    rizz__gfx_cmdbuffer* cb = (rizz__gfx_cmdbuffer*)rizz__core_gfx_cmdbuffer();

    sx_assert(cb->running_stage.id &&
              "draw related calls must come between begin_stage..end_stage");
    sx_assert(cb->cmd_idx < UINT16_MAX);

    int offset;
    uint8_t* buff = rizz__cb_alloc_params_buff(cb, sizeof(int) * 4 + sizeof(bool), &offset);
    sx_assert(buff);

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

static uint8_t* rizz__cb_run_apply_scissor_rect(uint8_t* buff)
{
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

static void rizz__cb_apply_pipeline(sg_pipeline pip)
{
    rizz__gfx_cmdbuffer* cb = (rizz__gfx_cmdbuffer*)rizz__core_gfx_cmdbuffer();

    sx_assert(cb->running_stage.id &&
              "draw related calls must come between begin_stage..end_stage");
    sx_assert(cb->cmd_idx < UINT16_MAX);

    int offset;
    uint8_t* buff = rizz__cb_alloc_params_buff(cb, sizeof(sg_pipeline), &offset);
    sx_assert(buff);

    rizz__gfx_cmdbuffer_ref ref = { .key =
                                        (((uint32_t)cb->stage_order << 16) | (uint32_t)cb->cmd_idx),
                                    .cmdbuffer_idx = cb->index,
                                    .cmd = GFX_COMMAND_APPLY_PIPELINE,
                                    .params_offset = offset };
    sx_array_push(cb->alloc, cb->refs, ref);

    ++cb->cmd_idx;

    *((sg_pipeline*)buff) = pip;

    _sg_pipeline_t* _pip = _sg_lookup_pipeline(&_sg.pools, pip.id);
    _pip->used_frame = _pip->shader->used_frame = the__core.frame_index();
}

static uint8_t* rizz__cb_run_apply_pipeline(uint8_t* buff)
{
    sg_pipeline pip_id = *((sg_pipeline*)buff);
    sg_apply_pipeline(pip_id);
    buff += sizeof(sg_pipeline);

    return buff;
}

static void rizz__cb_apply_bindings(const sg_bindings* bind)
{
    rizz__gfx_cmdbuffer* cb = (rizz__gfx_cmdbuffer*)rizz__core_gfx_cmdbuffer();

    sx_assert(cb->running_stage.id &&
              "draw related calls must come between begin_stage..end_stage");
    sx_assert(cb->cmd_idx < UINT16_MAX);

    int offset;
    uint8_t* buff = rizz__cb_alloc_params_buff(cb, sizeof(sg_bindings), &offset);
    sx_assert(buff);

    rizz__gfx_cmdbuffer_ref ref = { .key =
                                        (((uint32_t)cb->stage_order << 16) | (uint32_t)cb->cmd_idx),
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
            vb->used_frame = frame_idx;
        } else {
            break;
        }
    }

    if (bind->index_buffer.id) {
        _sg_buffer_t* ib = _sg_lookup_buffer(&_sg.pools, bind->index_buffer.id);
        ib->used_frame = frame_idx;
    }

    for (int i = 0; i < SG_MAX_SHADERSTAGE_IMAGES; i++) {
        if (bind->vs_images[i].id) {
            _sg_image_t* img = _sg_lookup_image(&_sg.pools, bind->vs_images[i].id);
            img->used_frame = frame_idx;
        } else {
            break;
        }
    }

    for (int i = 0; i < SG_MAX_SHADERSTAGE_BUFFERS; i++) {
        if (bind->vs_buffers[i].id) {
            _sg_buffer_t* buf = _sg_lookup_buffer(&_sg.pools, bind->vs_buffers[i].id);
            buf->used_frame = frame_idx;
        } else {
            break;
        }
    }

    for (int i = 0; i < SG_MAX_SHADERSTAGE_IMAGES; i++) {
        if (bind->fs_images[i].id) {
            _sg_image_t* img = _sg_lookup_image(&_sg.pools, bind->fs_images[i].id);
            img->used_frame = frame_idx;
        } else {
            break;
        }
    }

    for (int i = 0; i < SG_MAX_SHADERSTAGE_BUFFERS; i++) {
        if (bind->fs_buffers[i].id) {
            _sg_buffer_t* buf = _sg_lookup_buffer(&_sg.pools, bind->fs_buffers[i].id);
            buf->used_frame = frame_idx;
        } else {
            break;
        }
    }

    for (int i = 0; i < SG_MAX_SHADERSTAGE_IMAGES; i++) {
        if (bind->cs_images[i].id) {
            _sg_image_t* img = _sg_lookup_image(&_sg.pools, bind->cs_images[i].id);
            img->used_frame = frame_idx;
        } else {
            break;
        }
    }

    for (int i = 0; i < SG_MAX_SHADERSTAGE_BUFFERS; i++) {
        if (bind->cs_buffers[i].id) {
            _sg_buffer_t* buf = _sg_lookup_buffer(&_sg.pools, bind->cs_buffers[i].id);
            buf->used_frame = frame_idx;
        } else {
            break;
        }
    }

    for (int i = 0; i < SG_MAX_SHADERSTAGE_UAVS; i++) {
        if (bind->cs_buffer_uavs[i].id) {
            _sg_buffer_t* buf = _sg_lookup_buffer(&_sg.pools, bind->cs_buffer_uavs[i].id);
            buf->used_frame = frame_idx;
        } else {
            break;
        }
    }

    for (int i = 0; i < SG_MAX_SHADERSTAGE_UAVS; i++) {
        if (bind->cs_image_uavs[i].id) {
            _sg_image_t* img = _sg_lookup_image(&_sg.pools, bind->cs_image_uavs[i].id);
            img->used_frame = frame_idx;
        } else {
            break;
        }
    }
}

static uint8_t* rizz__cb_run_apply_bindings(uint8_t* buff)
{
    const sg_bindings* bindings = (const sg_bindings*)buff;
    sg_apply_bindings(bindings);
    buff += sizeof(sg_bindings);

    return buff;
}

static void rizz__cb_apply_uniforms(sg_shader_stage stage, int ub_index, const void* data,
                                    int num_bytes)
{
    rizz__gfx_cmdbuffer* cb = (rizz__gfx_cmdbuffer*)rizz__core_gfx_cmdbuffer();

    sx_assert(cb->running_stage.id &&
              "draw related calls must come between begin_stage..end_stage");
    sx_assert(cb->cmd_idx < UINT16_MAX);

    int offset;
    uint8_t* buff = rizz__cb_alloc_params_buff(
        cb, sizeof(sg_shader_stage) + sizeof(int) * 2 + num_bytes, &offset);
    sx_assert(buff);

    rizz__gfx_cmdbuffer_ref ref = { .key =
                                        (((uint32_t)cb->stage_order << 16) | (uint32_t)cb->cmd_idx),
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

static uint8_t* rizz__cb_run_apply_uniforms(uint8_t* buff)
{
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

static void rizz__cb_draw(int base_element, int num_elements, int num_instances)
{
    rizz__gfx_cmdbuffer* cb = (rizz__gfx_cmdbuffer*)rizz__core_gfx_cmdbuffer();

    sx_assert(cb->running_stage.id &&
              "draw related calls must come between begin_stage..end_stage");
    sx_assert(cb->cmd_idx < UINT16_MAX);

    int offset;
    uint8_t* buff = rizz__cb_alloc_params_buff(cb, sizeof(int) * 3, &offset);
    sx_assert(buff);

    rizz__gfx_cmdbuffer_ref ref = { .key =
                                        (((uint32_t)cb->stage_order << 16) | (uint32_t)cb->cmd_idx),
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

static uint8_t* rizz__cb_run_draw(uint8_t* buff)
{
    int base_element = *((int*)buff);
    buff += sizeof(int);
    int num_elements = *((int*)buff);
    buff += sizeof(int);
    int num_instances = *((int*)buff);
    buff += sizeof(int);
    sg_draw(base_element, num_elements, num_instances);
    return buff;
}

static void rizz__cb_dispatch(int thread_group_x, int thread_group_y, int thread_group_z)
{
    rizz__gfx_cmdbuffer* cb = (rizz__gfx_cmdbuffer*)rizz__core_gfx_cmdbuffer();

    sx_assert(cb->running_stage.id &&
              "draw related calls must come between begin_stage..end_stage");
    sx_assert(cb->cmd_idx < UINT16_MAX);

    int offset;
    uint8_t* buff = rizz__cb_alloc_params_buff(cb, sizeof(int) * 3, &offset);
    sx_assert(buff);

    rizz__gfx_cmdbuffer_ref ref = { .key =
                                        (((uint32_t)cb->stage_order << 16) | (uint32_t)cb->cmd_idx),
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

static uint8_t* rizz__cb_run_dispatch(uint8_t* buff)
{
    int thread_group_x = *((int*)buff);
    buff += sizeof(int);
    int thread_group_y = *((int*)buff);
    buff += sizeof(int);
    int thread_group_z = *((int*)buff);
    buff += sizeof(int);
    sg_dispatch(thread_group_x, thread_group_y, thread_group_z);
    return buff;
}


static void rizz__cb_end_pass()
{
    rizz__gfx_cmdbuffer* cb = (rizz__gfx_cmdbuffer*)rizz__core_gfx_cmdbuffer();

    sx_assert(cb->running_stage.id &&
              "draw related calls must come between begin_stage..end_stage");
    sx_assert(cb->cmd_idx < UINT16_MAX);

    rizz__gfx_cmdbuffer_ref ref = { .key =
                                        (((uint32_t)cb->stage_order << 16) | (uint32_t)cb->cmd_idx),
                                    .cmdbuffer_idx = cb->index,
                                    .cmd = GFX_COMMAND_END_PASS,
                                    .params_offset = sx_array_count(cb->params_buff) };
    sx_array_push(cb->alloc, cb->refs, ref);

    ++cb->cmd_idx;
}

static uint8_t* rizz__cb_run_end_pass(uint8_t* buff)
{
    sg_end_pass();
    return buff;
}

static void rizz__cb_update_buffer(sg_buffer buf, const void* data_ptr, int data_size)
{
    rizz__gfx_cmdbuffer* cb = (rizz__gfx_cmdbuffer*)rizz__core_gfx_cmdbuffer();

    sx_assert(cb->running_stage.id &&
              "draw related calls must come between begin_stage..end_stage");
    sx_assert(cb->cmd_idx < UINT16_MAX);

    int offset;
    uint8_t* buff =
        rizz__cb_alloc_params_buff(cb, sizeof(sg_buffer) + data_size + sizeof(int), &offset);
    sx_assert(buff);

    rizz__gfx_cmdbuffer_ref ref = { .key =
                                        (((uint32_t)cb->stage_order << 16) | (uint32_t)cb->cmd_idx),
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
    _buff->used_frame = the__core.frame_index();
}

static uint8_t* rizz__cb_run_update_buffer(uint8_t* buff)
{
    sg_buffer buf = *((sg_buffer*)buff);
    buff += sizeof(sg_buffer);
    int data_size = *((int*)buff);
    buff += sizeof(int);
    sg_update_buffer(buf, buff, data_size);
    buff += data_size;

    return buff;
}

static int rizz__cb_append_buffer(sg_buffer buf, const void* data_ptr, int data_size)
{
    // search for stream-buffer
    int index = -1;
    for (int i = 0, c = sx_array_count(g_gfx.stream_buffs); i < c; i++) {
        if (g_gfx.stream_buffs[i].buf.id == buf.id) {
            index = i;
            break;
        }
    }

    sx_assert(index != -1 && "buffer must be stream and not destroyed during render");
    rizz__gfx_stream_buffer* sbuff = &g_gfx.stream_buffs[index];
    sx_assert(sbuff->offset + data_size <= sbuff->size);
    int stream_offset = sx_atomic_fetch_add(&sbuff->offset, data_size);

    rizz__gfx_cmdbuffer* cb = (rizz__gfx_cmdbuffer*)rizz__core_gfx_cmdbuffer();

    sx_assert(cb->running_stage.id &&
              "draw related calls must come between begin_stage..end_stage");
    sx_assert(cb->cmd_idx < UINT16_MAX);

    int offset;
    uint8_t* buff =
        rizz__cb_alloc_params_buff(cb, data_size + sizeof(int) * 3 + sizeof(sg_buffer), &offset);
    sx_assert(buff);

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
    _buff->used_frame = the__core.frame_index();

    return stream_offset;
}

static uint8_t* rizz__cb_run_append_buffer(uint8_t* buff)
{
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
    sx_assert(sbuff->buf.id == buf.id &&
              "streaming buffers probably destroyed during render/update");
    sg_map_buffer(buf, stream_offset, buff, data_size);
    buff += data_size;

    return buff;
}

static void rizz__cb_update_image(sg_image img, const sg_image_content* data)
{
    rizz__gfx_cmdbuffer* cb = (rizz__gfx_cmdbuffer*)rizz__core_gfx_cmdbuffer();

    sx_assert(cb->running_stage.id &&
              "draw related calls must come between begin_stage..end_stage");
    sx_assert(cb->cmd_idx < UINT16_MAX);

    int image_size = 0;
    for (int face = 0; face < SG_CUBEFACE_NUM; face++) {
        for (int mip = 0; mip < SG_MAX_MIPMAPS; mip++) {
            image_size += data->subimage[face][mip].size;
        }
    }

    int offset;
    uint8_t* buff =
        rizz__cb_alloc_params_buff(cb, sizeof(sg_image) + sizeof(sg_image_content), &offset);
    sx_assert(buff);

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
    _img->used_frame = the__core.frame_index();
}

static uint8_t* rizz__cb_run_update_image(uint8_t* buff)
{
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

static void rizz__cb_begin_profile_sample(const char* name, uint32_t* hash_cache)
{
    rizz__gfx_cmdbuffer* cb = (rizz__gfx_cmdbuffer*)rizz__core_gfx_cmdbuffer();

    sx_assert(cb->running_stage.id &&
              "draw related calls must come between begin_stage..end_stage");
    sx_assert(cb->cmd_idx < UINT16_MAX);

    int offset;
    uint8_t* buff = rizz__cb_alloc_params_buff(cb, 32 + sizeof(uint32_t*), &offset);
    sx_assert(buff);

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
    const char* name = (const char*)buff;
    sx_unused(name);
    buff += 32;
    uint32_t* hash_cache = *((uint32_t**)buff);
    sx_unused(hash_cache);
    buff += sizeof(uint32_t*);
    rmt__begin_gpu_sample(name, hash_cache);
    return buff;
}

static void rizz__cb_end_profile_sample()
{
    rizz__gfx_cmdbuffer* cb = (rizz__gfx_cmdbuffer*)rizz__core_gfx_cmdbuffer();

    sx_assert(cb->running_stage.id &&
              "draw related calls must come between begin_stage..end_stage");
    sx_assert(cb->cmd_idx < UINT16_MAX);

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

rizz__gfx_cmdbuffer* rizz__gfx_create_command_buffer(const sx_alloc* alloc)
{
    rizz__gfx_cmdbuffer* cb = sx_malloc(alloc, sizeof(rizz__gfx_cmdbuffer));
    if (!cb) {
        sx_out_of_memory();
        return NULL;
    }

    *cb = (rizz__gfx_cmdbuffer){ .alloc = alloc, .index = sx_array_count(g_gfx.cmd_buffers) };

    sx_array_push(g_gfx_alloc, g_gfx.cmd_buffers, cb);
    return cb;
}

void rizz__gfx_destroy_command_buffer(rizz__gfx_cmdbuffer* cb)
{
    sx_assert(cb);
    sx_assert(cb->alloc);

    sx_array_pop(g_gfx.cmd_buffers, cb->index);

    sx_array_free(cb->alloc, cb->params_buff);
    sx_array_free(cb->alloc, cb->refs);
    sx_free(cb->alloc, cb);
}

static const rizz__run_command_cb k_run_cbs[_GFX_COMMAND_COUNT] = {
    rizz__cb_run_begin_default_pass, rizz__cb_run_begin_pass,
    rizz__cb_run_apply_viewport,     rizz__cb_run_apply_scissor_rect,
    rizz__cb_run_apply_pipeline,     rizz__cb_run_apply_bindings,
    rizz__cb_run_apply_uniforms,     rizz__cb_run_draw,
    rizz__cb_run_dispatch,           rizz__cb_run_end_pass,
    rizz__cb_run_update_buffer,      rizz__cb_run_update_image,
    rizz__cb_run_append_buffer,      rizz__cb_run_begin_profile_sample,
    rizz__cb_run_end_profile_sample, rizz__cb_run_begin_stage,
    rizz__cb_run_end_stage
};

// note: must run in main thread
void rizz__gfx_execute_command_buffers()
{
    static_assert((sizeof(k_run_cbs) / sizeof(rizz__run_command_cb)) == _GFX_COMMAND_COUNT,
                  "k_run_cbs must match rizz__gfx_command");

    // gather all command buffers that submitted a command
    const sx_alloc* tmp_alloc = the__core.tmp_alloc_push();
    int cmd_count = 0;
    for (int i = 0, c = sx_array_count(g_gfx.cmd_buffers); i < c; i++) {
        rizz__gfx_cmdbuffer* cb = g_gfx.cmd_buffers[i];
        sx_assert(cb->running_stage.id == 0 &&
                  "all command buffers must fully submit their calls and call end_stage");
        cmd_count += sx_array_count(cb->refs);
    }

    //
    if (cmd_count) {
        rizz__gfx_cmdbuffer_ref* refs =
            sx_malloc(tmp_alloc, sizeof(rizz__gfx_cmdbuffer_ref) * cmd_count);
        sx_assert(refs);

        rizz__gfx_cmdbuffer_ref* init_refs = refs;
        for (int i = 0, c = sx_array_count(g_gfx.cmd_buffers); i < c; i++) {
            rizz__gfx_cmdbuffer* cb = g_gfx.cmd_buffers[i];
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
            rizz__gfx_cmdbuffer* cb = g_gfx.cmd_buffers[ref->cmdbuffer_idx];
            k_run_cbs[ref->cmd](&cb->params_buff[ref->params_offset]);
        }

        sx_free(tmp_alloc, refs);
    }

    // reset param buffers
    for (int i = 0, c = sx_array_count(g_gfx.cmd_buffers); i < c; i++) {
        sx_array_clear(g_gfx.cmd_buffers[i]->params_buff);
        g_gfx.cmd_buffers[i]->cmd_idx = 0;
    }

    // clear all stages
    for (int i = 0, c = sx_array_count(g_gfx.stages); i < c; i++) {
        g_gfx.stages[i].state = STAGE_STATE_NONE;
    }

    // clear stream buffer offsets
    for (int i = 0, c = sx_array_count(g_gfx.stream_buffs); i < c; i++) {
        g_gfx.stream_buffs[i].offset = 0;
    }

    the__core.tmp_alloc_pop();
}

static rizz_gfx_stage rizz__stage_register(const char* name, rizz_gfx_stage parent_stage)
{
    sx_assert(name);
    sx_assert(parent_stage.id == 0 || parent_stage.id < (uint32_t)sx_array_count(g_gfx.stages));
    sx_assert(sx_array_count(g_gfx.stages) < MAX_STAGES && "maximum stages exceeded");

    rizz__gfx_stage _stage = { .name_hash = sx_hash_fnv32_str(name),
                               .parent = parent_stage,
                               .enabled = 1,
                               .single_enabled = 1 };
    sx_strcpy(_stage.name, sizeof(_stage.name), name);

    rizz_gfx_stage stage = { .id = rizz_to_id(sx_array_count(g_gfx.stages)) };
    sx_array_push(g_gfx_alloc, g_gfx.stages, _stage);

    // add to dependency graph
    if (parent_stage.id)
        rizz__stage_add_child(parent_stage, stage);

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
    sx_assert(depth < MAX_DEPTH && "maximum stage dependency depth exceeded");

    _stage.order = ((depth << STAGE_ORDER_DEPTH_BITS) & STAGE_ORDER_DEPTH_MASK) |
                   (uint16_t)(rizz_to_index(stage.id) & STAGE_ORDER_ID_MASK);

    return stage;
}

static void rizz__stage_enable(rizz_gfx_stage stage)
{
    sx_assert(stage.id);

    sx_lock(&g_gfx.stage_lk, 1);
    rizz__gfx_stage* _stage = &g_gfx.stages[rizz_to_index(stage.id)];
    _stage->enabled = 1;
    _stage->single_enabled = 1;

    // apply for children
    for (rizz_gfx_stage child = _stage->child; child.id;
         child = g_gfx.stages[rizz_to_index(child.id)].next) {
        rizz__gfx_stage* _child = &g_gfx.stages[rizz_to_index(child.id)];
        _child->enabled = _child->single_enabled;
    }
    sx_unlock(&g_gfx.stage_lk);
}

static void rizz__stage_disable(rizz_gfx_stage stage)
{
    sx_assert(stage.id);

    sx_lock(&g_gfx.stage_lk, 1);
    rizz__gfx_stage* _stage = &g_gfx.stages[rizz_to_index(stage.id)];
    _stage->enabled = 0;
    _stage->single_enabled = 0;

    // apply for children
    for (rizz_gfx_stage child = _stage->child; child.id;
         child = g_gfx.stages[rizz_to_index(child.id)].next) {
        rizz__gfx_stage* _child = &g_gfx.stages[rizz_to_index(child.id)];
        _child->enabled = 0;
    }
    sx_unlock(&g_gfx.stage_lk);
}

static bool rizz__stage_isenabled(rizz_gfx_stage stage)
{
    sx_assert(stage.id);

    sx_lock(&g_gfx.stage_lk, 1);
    bool enabled = g_gfx.stages[rizz_to_index(stage.id)].enabled;
    sx_unlock(&g_gfx.stage_lk);
    return enabled;
}

static rizz_gfx_stage rizz__stage_find(const char* name)
{
    sx_assert(name);

    uint32_t name_hash = sx_hash_fnv32_str(name);
    sx_lock(&g_gfx.stage_lk, 1);
    for (int i = 0, c = sx_array_count(g_gfx.stages); i < c; i++) {
        if (g_gfx.stages[i].name_hash == name_hash)
            return (rizz_gfx_stage){ .id = rizz_to_id(i) };
    }
    sx_unlock(&g_gfx.stage_lk);
    return (rizz_gfx_stage){ .id = -1 };
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
    sg_pipeline pip_id = sg_make_pipeline(desc);
#if RIZZ_CONFIG_HOT_LOADING
#    if defined(SOKOL_METAL)
    rizz__pip_mtl pip = { .pip = pip_id, .desc = *desc };
    sx_array_push(g_gfx_alloc, g_gfx.pips, pip);
#    else
    sx_array_push(g_gfx_alloc, g_gfx.pips, pip_id);
#    endif
#endif

    return pip_id;
}

static void rizz__destroy_pipeline(sg_pipeline pip_id)
{
    rizz__queue_destroy(g_gfx.destroy_pips, pip_id, g_gfx_alloc);
}

static void rizz__destroy_shader(sg_shader shd_id)
{
    rizz__queue_destroy(g_gfx.destroy_shaders, shd_id, g_gfx_alloc);
}

static void rizz__destroy_pass(sg_pass pass_id)
{
    rizz__queue_destroy(g_gfx.destroy_passes, pass_id, g_gfx_alloc);
}

static void rizz__destroy_image(sg_image img_id)
{
    rizz__queue_destroy(g_gfx.destroy_images, img_id, g_gfx_alloc);
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
    rizz__queue_destroy(g_gfx.destroy_buffers, buf_id, g_gfx_alloc);
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

static void rizz__debug_grid_xzplane(float spacing, float spacing_bold, const sx_mat4* vp,
                                     const sx_vec3 frustum[8])
{
    static const sx_color color = { { 170, 170, 170, 255 } };
    static const sx_color bold_color = { { 255, 255, 255, 255 } };

    spacing = sx_ceil(sx_max(spacing, 0.0001f));
    sx_aabb bb = sx_aabb_empty();

    // extrude near plane
    sx_vec3 near_plane_norm = sx_vec3_calc_normal(frustum[0], frustum[1], frustum[2]);
    for (int i = 0; i < 8; i++) {
        if (i < 4) {
            sx_vec3 offset_pt = sx_vec3_sub(frustum[i], sx_vec3_mulf(near_plane_norm, spacing));
            sx_aabb_add_point(&bb, sx_vec3f(offset_pt.x, 0, offset_pt.z));
        } else {
            sx_aabb_add_point(&bb, sx_vec3f(frustum[i].x, 0, frustum[i].z));
        }
    }

    // snap grid bounds to `spacing`
    int nspace = (int)spacing;
    sx_aabb snapbox = sx_aabbf((float)((int)bb.xmin - (int)bb.xmin % nspace), 0,
                               (float)((int)bb.zmin - (int)bb.zmin % nspace),
                               (float)((int)bb.xmax - (int)bb.xmax % nspace), 0,
                               (float)((int)bb.zmax - (int)bb.zmax % nspace));
    float w = snapbox.xmax - snapbox.xmin;
    float d = snapbox.zmax - snapbox.zmin;
    if (sx_equal(w, 0, 0.00001f) || sx_equal(d, 0, 0.00001f))
        return;

    int xlines = (int)w / nspace + 1;
    int ylines = (int)d / nspace + 1;
    int num_verts = (xlines + ylines) * 2;

    // draw
    int data_size = num_verts * sizeof(rizz__debug_vertex);
    const sx_alloc* tmp_alloc = the__core.tmp_alloc_push();
    rizz__debug_vertex* verts = sx_malloc(tmp_alloc, data_size);
    sx_assert(verts);

    int i = 0;
    for (float zoffset = snapbox.zmin; zoffset <= snapbox.zmax; zoffset += spacing, i += 2) {
        verts[i].pos.x = snapbox.xmin;
        verts[i].pos.y = 0;
        verts[i].pos.z = zoffset;

        int ni = i + 1;
        verts[ni].pos.x = snapbox.xmax;
        verts[ni].pos.y = 0;
        verts[ni].pos.z = zoffset;

        verts[i].color = verts[ni].color =
            (zoffset != 0.0f)
                ? (!sx_equal(sx_mod(zoffset, spacing_bold), 0.0f, 0.0001f) ? color : bold_color)
                : SX_COLOR_RED;
    }

    for (float xoffset = snapbox.xmin; xoffset <= snapbox.xmax; xoffset += spacing, i += 2) {
        verts[i].pos.x = xoffset;
        verts[i].pos.y = 0;
        verts[i].pos.z = snapbox.zmin;

        int ni = i + 1;
        sx_assert(ni < num_verts);
        verts[ni].pos.x = xoffset;
        verts[ni].pos.y = 0;
        verts[ni].pos.z = snapbox.zmax;

        verts[i].color = verts[ni].color =
            (xoffset != 0.0f)
                ? (!sx_equal(sx_mod(xoffset, spacing_bold), 0.0f, 0.0001f) ? color : bold_color)
                : SX_COLOR_BLUE;
    }

    int offset = the__gfx.staged.append_buffer(g_gfx.dbg.vb, verts, data_size);
    sg_bindings bind = { .vertex_buffers[0] = g_gfx.dbg.vb, .vertex_buffer_offsets[0] = offset };
    rizz__debug_uniforms uniforms = { .model = sx_mat4_ident(), .vp = *vp };

    the__gfx.staged.apply_pipeline(g_gfx.dbg.pip_wire);
    the__gfx.staged.apply_uniforms(SG_SHADERSTAGE_VS, 0, &uniforms, sizeof(uniforms));
    the__gfx.staged.apply_bindings(&bind);
    the__gfx.staged.draw(0, num_verts, 1);

    the__core.tmp_alloc_pop();
}

void rizz__debug_grid_xyplane(float spacing, float spacing_bold, const sx_mat4* vp,
                              const sx_vec3 frustum[8])
{
    static const sx_color color = { { 170, 170, 170, 255 } };
    static const sx_color bold_color = { { 255, 255, 255, 255 } };

    spacing = sx_ceil(sx_max(spacing, 0.0001f));
    sx_aabb bb = sx_aabb_empty();

    // extrude near plane
    sx_vec3 near_plane_norm = sx_vec3_calc_normal(frustum[0], frustum[1], frustum[2]);
    for (int i = 0; i < 8; i++) {
        if (i < 4) {
            sx_vec3 offset_pt = sx_vec3_sub(frustum[i], sx_vec3_mulf(near_plane_norm, spacing));
            sx_aabb_add_point(&bb, sx_vec3f(offset_pt.x, offset_pt.y, 0));
        } else {
            sx_aabb_add_point(&bb, sx_vec3f(frustum[i].x, frustum[i].y, 0));
        }
    }

    // snap grid bounds to `spacing`
    int nspace = (int)spacing;
    sx_aabb snapbox = sx_aabbf((float)((int)bb.xmin - (int)bb.xmin % nspace),
                               (float)((int)bb.ymin - (int)bb.ymin % nspace), 0,
                               (float)((int)bb.xmax - (int)bb.xmax % nspace),
                               (float)((int)bb.ymax - (int)bb.ymax % nspace), 0);
    float w = snapbox.xmax - snapbox.xmin;
    float h = snapbox.ymax - snapbox.ymin;
    if (sx_equal(w, 0, 0.00001f) || sx_equal(h, 0, 0.00001f))
        return;

    int xlines = (int)w / nspace + 1;
    int ylines = (int)h / nspace + 1;
    int num_verts = (xlines + ylines) * 2;

    // draw
    int data_size = num_verts * sizeof(rizz__debug_vertex);
    const sx_alloc* tmp_alloc = the__core.tmp_alloc_push();
    rizz__debug_vertex* verts = sx_malloc(tmp_alloc, data_size);
    sx_assert(verts);

    int i = 0;
    for (float yoffset = snapbox.ymin; yoffset <= snapbox.ymax; yoffset += spacing, i += 2) {
        verts[i].pos.x = snapbox.xmin;
        verts[i].pos.y = yoffset;
        verts[i].pos.z = 0;

        int ni = i + 1;
        verts[ni].pos.x = snapbox.xmax;
        verts[ni].pos.y = yoffset;
        verts[ni].pos.z = 0;

        verts[i].color = verts[ni].color =
            (yoffset != 0.0f)
                ? (!sx_equal(sx_mod(yoffset, spacing_bold), 0.0f, 0.0001f) ? color : bold_color)
                : SX_COLOR_RED;
    }

    for (float xoffset = snapbox.xmin; xoffset <= snapbox.xmax; xoffset += spacing, i += 2) {
        verts[i].pos.x = xoffset;
        verts[i].pos.y = snapbox.ymin;
        verts[i].pos.z = 0;

        int ni = i + 1;
        sx_assert(ni < num_verts);
        verts[ni].pos.x = xoffset;
        verts[ni].pos.y = snapbox.ymax;
        verts[ni].pos.z = 0;

        verts[i].color = verts[ni].color =
            (xoffset != 0.0f)
                ? (!sx_equal(sx_mod(xoffset, spacing_bold), 0.0f, 0.0001f) ? color : bold_color)
                : SX_COLOR_GREEN;
    }

    int offset = the__gfx.staged.append_buffer(g_gfx.dbg.vb, verts, data_size);
    sg_bindings bind = { .vertex_buffers[0] = g_gfx.dbg.vb, .vertex_buffer_offsets[0] = offset };
    rizz__debug_uniforms uniforms = { .model = sx_mat4_ident(), .vp = *vp };

    the__gfx.staged.apply_pipeline(g_gfx.dbg.pip_wire);
    the__gfx.staged.apply_uniforms(SG_SHADERSTAGE_VS, 0, &uniforms, sizeof(uniforms));
    the__gfx.staged.apply_bindings(&bind);
    the__gfx.staged.draw(0, num_verts, 1);
    the__core.tmp_alloc_pop();
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

static bool rizz__imm_begin(rizz_gfx_stage stage) 
{
    sx_unused(stage);
    return true;
}

static void rizz__imm_end() {}

// clang-format off
rizz_api_gfx the__gfx = {
    .imm = { 
             .begin                 = rizz__imm_begin,
             .end                   = rizz__imm_end,
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
             .end_profile_sample    = rizz__end_profile_sample },
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
                .end_profile_sample   = rizz__cb_end_profile_sample },
    .backend                    = rizz__gfx_backend,
    .GL_family                  = rizz__gfx_GL_family,
    .reset_state_cache          = sg_reset_state_cache,
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
    .texture_white              = rizz__texture_white,
    .texture_black              = rizz__texture_black,
    .texture_checker            = rizz__texture_checker,
    .texture_create_checker     = rizz__texture_create_checker,
    .debug_grid_xzplane         = rizz__debug_grid_xzplane,
    .debug_grid_xyplane         = rizz__debug_grid_xyplane,
    .trace_info                 = rizz__trace_info
};
// clang-format on
