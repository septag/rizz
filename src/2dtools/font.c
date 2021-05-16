#include "rizz/2dtools.h"

#include "sx/array.h"
#include "sx/math-scalar.h"
#include "sx/os.h"
#include "sx/string.h"

#include "rizz/imgui.h"

#include "dummy-font.h"

#define FONS_STATIC
#define FONS_MALLOC(alloc, sz) sx_malloc((const sx_alloc*)alloc, sz)
#define FONS_REALLOC(alloc, ptr, sz) sx_realloc((const sx_alloc*)alloc, ptr, sz)
#define FONS_FREE(alloc, ptr) sx_free((const sx_alloc*)alloc, ptr)
#define FONTSTASH_IMPLEMENTATION
SX_PRAGMA_DIAGNOSTIC_PUSH()
SX_PRAGMA_DIAGNOSTIC_IGNORED_CLANG("-Wunused-function")
#include "fontstash/fontstash.h"
SX_PRAGMA_DIAGNOSTIC_POP()

#include rizz_shader_path(shaders_h, font.vert.h)
#include rizz_shader_path(shaders_h, font.frag.h)

#define MAX_VERTICES 4000
#define MAX_INDICES 12000

RIZZ_STATE static rizz_api_core* the_core;
RIZZ_STATE static rizz_api_asset* the_asset;
RIZZ_STATE static rizz_api_gfx* the_gfx;
RIZZ_STATE static rizz_api_imgui* the_imgui;
RIZZ_STATE static rizz_api_app* the_app;

typedef struct font__fons {
    rizz_font f;
    FONScontext* ctx;
    char name[32];
    bool img_dirty;
    bool ignore_dpiscale;
} font__fons;

typedef struct font__context {
    const sx_alloc* alloc;
    rizz_api_gfx_draw* draw_api;
    font__fons** fonts;    // sx_array
    sg_pipeline pip;
    sg_buffer vbuff;
    sg_shader shader;
    font__fons font_async;
    font__fons font_failed;
} font__context;

static rizz_vertex_layout k_font_vertex_layout = {
    .attrs[0] = { .semantic = "POSITION", .offset = offsetof(rizz_font_vertex, pos) },
    .attrs[1] = { .semantic = "TEXCOORD", .offset = offsetof(rizz_font_vertex, uv) },
    .attrs[2] = { .semantic = "COLOR",
                  .offset = offsetof(rizz_sprite_vertex, color),
                  .format = SG_VERTEXFORMAT_UBYTE4N }
};

RIZZ_STATE static font__context g_font;

static int fons__create_fn(void* user_ptr, int width, int height)
{
    font__fons* fons = user_ptr;
    sx_assert(fons->f.img_atlas.id == 0);

    // create font texture
    fons->f.img_atlas = the_gfx->make_image(&(sg_image_desc){ .width = width,
                                                              .height = height,
                                                              .min_filter = SG_FILTER_LINEAR,
                                                              .mag_filter = SG_FILTER_LINEAR,
                                                              .usage = SG_USAGE_DYNAMIC,
                                                              .pixel_format = SG_PIXELFORMAT_R8,
                                                              .label = fons->name});

    if (!fons->f.img_atlas.id) {
        return 0;
    }

    return 1;
}

static int fons__resize_fn(void* user_ptr, int width, int height)
{
    font__fons* fons = user_ptr;

    if (fons->f.img_atlas.id) {
        the_gfx->destroy_image(fons->f.img_atlas);
        fons->f.img_atlas.id = 0;
    }

    if (fons__create_fn(user_ptr, width, height)) {
        fons->f.img_width = width;
        fons->f.img_height = height;
        return 1;
    } else {
        return 0;
    }
}

static void fons__update_fn(void* user_ptr, int* rect, const unsigned char* data)
{
    sx_assert(user_ptr && rect && data);
    sx_unused(rect);
    sx_unused(data);

    font__fons* fons = user_ptr;
    fons->img_dirty = true;
}

static void fons__draw_fn(void* user_ptr, const float* poss, const float* tcoords,
                          const unsigned int* colors, int nverts)
{
    font__fons* fons = user_ptr;
    rizz_api_gfx_draw* draw_api = g_font.draw_api;

    const sx_alloc* tmp_alloc = the_core->tmp_alloc_push();
    sx_scope(the_core->tmp_alloc_pop()) {
        rizz_font_vertex* verts = sx_malloc(tmp_alloc, sizeof(rizz_font_vertex) * nverts);
        sx_assert(verts);

        sx_assert(nverts % 3 == 0);
        int ntris = nverts / 3;

        // flip the orders of each triangle, because Y is reversed (thus the triangle ordering)
        // so the flip the triangle to properly cull with BACK
        for (int i = 0; i < ntris; i++) {
            int vindex = i * 3;

            int findex = vindex * 2;
            verts[vindex + 2].pos = sx_vec2f(poss[findex], poss[findex + 1]);
            verts[vindex + 2].uv = sx_vec2f(tcoords[findex], tcoords[findex + 1]);
            verts[vindex + 2].color = sx_colorn(colors[i]);

            findex += 2;
            verts[vindex + 1].pos = sx_vec2f(poss[findex], poss[findex + 1]);
            verts[vindex + 1].uv = sx_vec2f(tcoords[findex], tcoords[findex + 1]);
            verts[vindex + 1].color = sx_colorn(colors[i]);

            findex += 2;
            verts[vindex].pos = sx_vec2f(poss[findex], poss[findex + 1]);
            verts[vindex].uv = sx_vec2f(tcoords[findex], tcoords[findex + 1]);
            verts[vindex].color = sx_colorn(colors[i]);
        }

        int vb_offset = draw_api->append_buffer(g_font.vbuff, verts, sizeof(rizz_font_vertex) * nverts);

        sg_bindings bindings = { .vertex_buffers[0] = g_font.vbuff,
                                 .vertex_buffer_offsets[0] = vb_offset,
                                 .fs_images[0] = fons->f.img_atlas };

        FONSstate* state = fons__getState(fons->ctx);

        draw_api->apply_pipeline(g_font.pip);
        draw_api->apply_bindings(&bindings);
        if (state->scissor[2] > 0 && state->scissor[3] > 0) {
            draw_api->apply_scissor_rect(state->scissor[0], state->scissor[1], state->scissor[2],
                                         state->scissor[3], !the_gfx->GL_family());
        }
        draw_api->apply_uniforms(SG_SHADERSTAGE_VS, 0, state->mat, sizeof(state->mat));
        draw_api->draw(0, nverts, 1);
    } // scope
}

static void fons__delete_fn(void* user_ptr)
{
    font__fons* fons = user_ptr;

    if (fons->f.img_atlas.id) {
        the_gfx->destroy_image(fons->f.img_atlas);
        fons->f.img_atlas.id = 0;
    }
}

static void fons__error_fn(void* user_ptr, int error, int val)
{
    sx_unused(val);
    font__fons* fons = user_ptr;

    switch (error) {
    case FONS_ATLAS_FULL:
        // double the size
        if (!fonsExpandAtlas(fons->ctx, fons->f.img_width << 1, fons->f.img_height << 1)) {
            rizz_log_error("font: failed to resize atlas for '%s'", fons->name);
        }
        rizz_log_warn("font: '%s' atlas size expanded to (%d, %d)", fons->name, fons->f.img_width,
                      fons->f.img_height);
        break;
    case FONS_SCRATCH_FULL:
        rizz_log_error("font: scratch memory for glyphs '%s' is full", fons->name);
        break;
    case FONS_STATES_OVERFLOW:
        rizz_log_error("font: push_states overflow for '%s'", fons->name);
        break;
    case FONS_STATES_UNDERFLOW:
        rizz_log_error("font: pop_states underflow for '%s'", fons->name);
        break;
    }
}

static rizz_asset_load_data font__fons_on_prepare(const rizz_asset_load_params* params,
                                                  const sx_mem_block* mem)
{
    const sx_alloc* alloc = params->alloc ? params->alloc : g_font.alloc;
    const rizz_font_load_params* fparams = params->params;

    font__fons* fons = sx_malloc(alloc, sizeof(font__fons));
    if (!fons) {
        sx_out_of_memory();
        return (rizz_asset_load_data){ .obj = { 0 } };
    }

    float dpiscale = fparams->ignore_dpiscale ? 1.0f : the_app->dpiscale();
    int atlas_width = fparams->atlas_width > 0
                          ? (int)((float)fparams->atlas_width * dpiscale)
                          : sx_nearest_pow2((int)(512.0f * dpiscale));
    int atlas_height = fparams->atlas_height > 0
                           ? (int)((float)fparams->atlas_height * dpiscale)
                           : sx_nearest_pow2((int)(512.0f * dpiscale));
    fons->f.img_width = atlas_width;
    fons->f.img_height = atlas_height;
    fons->f.img_atlas.id = 0;
    fons->ignore_dpiscale = fparams->ignore_dpiscale;

    char name[64];
    sx_os_path_basename(name, sizeof(name), params->path);
    sx_strcpy(fons->name, sizeof(fons->name), name);

    fons->ctx = fonsCreateInternal(&(FONSparams){ .width = atlas_width,
                                                  .height = atlas_height,
                                                  .flags = FONS_ZERO_TOPLEFT,
                                                  .userPtr = fons,
                                                  .allocPtr = (void*)alloc,
                                                  .renderCreate = fons__create_fn,
                                                  .renderDelete = fons__delete_fn,
                                                  .renderDraw = fons__draw_fn,
                                                  .renderUpdate = fons__update_fn,
                                                  .renderResize = fons__resize_fn });
    if (!fons->ctx) {
        return (rizz_asset_load_data){ .obj = { 0 } };
    }

    fonsSetErrorCallback(fons->ctx, fons__error_fn, fons);

    void* buffer = sx_malloc(alloc, (size_t)mem->size);
    if (!buffer) {
        sx_out_of_memory();
        return (rizz_asset_load_data){ .obj = { 0 } };
    }

    return (rizz_asset_load_data){ .obj = { .ptr = fons }, .user1 = buffer };
}

static bool font__fons_on_load(rizz_asset_load_data* data, const rizz_asset_load_params* params,
                               const sx_mem_block* mem)
{
    font__fons* fons = data->obj.ptr;

    char name[64];
    sx_os_path_basename(name, sizeof(name), params->path);

    sx_strcpy(fons->name, sizeof(fons->name), name);

    sx_memcpy(data->user1, mem->data, (size_t)mem->size);

    int fons_id = fonsAddFontMem(fons->ctx, name, data->user1, (int)mem->size, 1);
    if (fons_id == FONS_INVALID) {
        rizz_log_warn("loading font '%s' failed: invalid TTF file", params->path);
        return false;
    }

    fonsSetFont(fons->ctx, fons_id);
    fonsSetSize(fons->ctx, 12.0f * (fons->ignore_dpiscale ? 1.0f : the_app->dpiscale()));

    return true;
}

static void font__fons_on_finalize(rizz_asset_load_data* data, const rizz_asset_load_params* params,
                                   const sx_mem_block* mem)
{
    sx_unused(params);
    sx_unused(mem);

    // add font to database to texture updates
    font__fons* font = data->obj.ptr;
    sx_assert(font);
    sx_array_push(g_font.alloc, g_font.fonts, font);
}

static void font__fons_on_reload(rizz_asset handle, rizz_asset_obj prev_obj, const sx_alloc* alloc)
{
    sx_unused(handle);
    sx_unused(prev_obj);
    sx_unused(alloc);
}

static void font__fons_on_release(rizz_asset_obj obj, const sx_alloc* alloc)
{
    font__fons* fons = obj.ptr;
    sx_assert(fons);

    if (!alloc) {
        alloc = g_font.alloc;
    }

    fonsDeleteInternal(fons->ctx);

    // remove from font database, so we don't have to update it anymore
    for (int i = 0, c = sx_array_count(g_font.fonts); i < c; i++) {
        if (g_font.fonts[i] == fons) {
            sx_array_pop(g_font.fonts, i);
            break;
        }
    }

    sx_free(alloc, fons);
}

bool font__resize_draw_limits(int max_verts)
{
    sx_assert(max_verts < UINT16_MAX);

    // recreate vertex/index buffers
    if (g_font.vbuff.id)
        the_gfx->destroy_buffer(g_font.vbuff);

    if (max_verts == 0) {
        g_font.vbuff = (sg_buffer){ 0 };
        return true;
    }

    g_font.vbuff = the_gfx->make_buffer(&(sg_buffer_desc)
        { .size = sizeof(rizz_sprite_vertex) * max_verts,
          .usage = SG_USAGE_STREAM,
          .type = SG_BUFFERTYPE_VERTEXBUFFER,
          .label = "font_vbuff"
    });

    return g_font.vbuff.id;
}

void font__update(void)
{
    for (int i = 0, c = sx_array_count(g_font.fonts); i < c; i++) {
        font__fons* font = g_font.fonts[i];
        if (font->img_dirty) {
            font->img_dirty = false;
            the_gfx->imm.update_image(
                font->f.img_atlas,
                &(sg_image_content){ .subimage[0][0].ptr = font->ctx->texData,
                                     .subimage[0][0].size =
                                         font->f.img_width * font->f.img_height });
        }
    }
}

bool font__init(rizz_api_core* core, rizz_api_asset* asset, rizz_api_gfx* gfx, rizz_api_app* app)
{
    the_core = core;
    the_asset = asset;
    the_gfx = gfx;
    the_app = app;
    
    g_font.alloc = the_core->alloc(RIZZ_MEMID_GRAPHICS);
    g_font.draw_api = &the_gfx->staged;

    // gfx objects
    const sx_alloc* tmp_alloc = the_core->tmp_alloc_push();
    sx_scope(the_core->tmp_alloc_pop()) {
        rizz_shader shader = the_gfx->shader_make_with_data(
            tmp_alloc, k_font_vs_size, k_font_vs_data, k_font_vs_refl_size, k_font_vs_refl_data,
            k_font_fs_size, k_font_fs_data, k_font_fs_refl_size, k_font_fs_refl_data);
        g_font.shader = shader.shd;

        // note: when we make the geometry, we modify the
        sg_pipeline_desc pip_desc =
            (sg_pipeline_desc){ .layout.buffers[0].stride = sizeof(rizz_font_vertex),
                                .rasterizer = { .cull_mode = SG_CULLMODE_BACK },
                                .blend = { .enabled = true,
                                           .src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA,
                                           .dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA },
                                .label = "rizz_font" };

        g_font.pip = the_gfx->make_pipeline(
            the_gfx->shader_bindto_pipeline(&shader, &pip_desc, &k_font_vertex_layout));
    } // scope


    if (!font__resize_draw_limits(MAX_VERTICES)) {
        return false;
    }

    // initialize not_found/async dummy object
    const int dummy_texture_size = 256;
    FONSparams fparams = { .width = dummy_texture_size,
                           .height = dummy_texture_size,
                           .flags = FONS_ZERO_TOPLEFT,
                           .allocPtr = (void*)g_font.alloc,
                           .renderCreate = fons__create_fn,
                           .renderDelete = fons__delete_fn,
                           .renderDraw = fons__draw_fn,
                           .renderUpdate = fons__update_fn };

    fparams.userPtr = &g_font.font_failed;
    g_font.font_failed.f.img_width = dummy_texture_size;
    g_font.font_failed.f.img_height = dummy_texture_size;
    sx_strcpy(g_font.font_failed.name, sizeof(g_font.font_failed.name), "font_failed");
    g_font.font_failed.ctx = fonsCreateInternal(&fparams);

    fparams.userPtr = &g_font.font_async;
    g_font.font_async.f.img_width = dummy_texture_size;
    g_font.font_async.f.img_height = dummy_texture_size;
    sx_strcpy(g_font.font_async.name, sizeof(g_font.font_async.name), "font_async");
    g_font.font_async.ctx = fonsCreateInternal(&fparams);
    if (!g_font.font_failed.ctx || !g_font.font_async.ctx) {
        return false;
    }

    fonsAddFontMem(g_font.font_failed.ctx, "font_failed", (unsigned char*)k_dummy_font,
                   sizeof(k_dummy_font), 0);
    fonsSetErrorCallback(g_font.font_failed.ctx, fons__error_fn, &g_font.font_failed);
    fonsSetColor(g_font.font_failed.ctx, 0xffff00ff);
    sx_array_push(g_font.alloc, g_font.fonts, &g_font.font_failed);

    fonsAddFontMem(g_font.font_async.ctx, "font_async", (unsigned char*)k_dummy_font,
                   sizeof(k_dummy_font), 0);
    fonsSetErrorCallback(g_font.font_async.ctx, fons__error_fn, &g_font.font_async);
    fonsSetColor(g_font.font_async.ctx, 0);
    sx_array_push(g_font.alloc, g_font.fonts, &g_font.font_async);

    the_asset->register_asset_type("font",
                                   (rizz_asset_callbacks){ .on_prepare = font__fons_on_prepare,
                                                           .on_load = font__fons_on_load,
                                                           .on_finalize = font__fons_on_finalize,
                                                           .on_reload = font__fons_on_reload,
                                                           .on_release = font__fons_on_release },
                                   "rizz_font_load_params", sizeof(rizz_font_load_params),
                                   (rizz_asset_obj){ .ptr = &g_font.font_failed },
                                   (rizz_asset_obj){ .ptr = &g_font.font_async }, 0);

    return true;
}

void font__set_imgui(rizz_api_imgui* imgui)
{
    the_imgui = imgui;
}

void font__release(void)
{
    the_asset->unregister_asset_type("font");

    // destroy dummy font objects
    if (g_font.font_async.ctx) {
        fonsDeleteInternal(g_font.font_async.ctx);
    }

    if (g_font.font_failed.ctx) {
        fonsDeleteInternal(g_font.font_failed.ctx);
    }

    // destroy gfx objects
    if (g_font.vbuff.id)
        the_gfx->destroy_buffer(g_font.vbuff);
    if (g_font.shader.id)
        the_gfx->destroy_shader(g_font.shader);
    if (g_font.pip.id)
        the_gfx->destroy_pipeline(g_font.pip);

    sx_array_free(g_font.alloc, g_font.fonts);
}

const rizz_font* font__get(rizz_asset font_asset)
{
#if RIZZ_DEV_BUILD
    sx_assert_always(sx_strequal(the_asset->type_name(font_asset), "font") && "asset handle is not a font");
#endif
    return (const rizz_font*)the_asset->obj(font_asset).ptr;
}

void font__draw(const rizz_font* fnt, sx_vec2 pos, const char* text)
{
    const font__fons* fons = (const font__fons*)fnt;
    fonsDrawText(fons->ctx, pos.x, pos.y, text, NULL);
}

void font__drawf(const rizz_font* fnt, sx_vec2 pos, const char* fmt, ...)
{
    char text[1024];
    va_list args;
    va_start(args, fmt);
    sx_vsnprintf(text, sizeof(text), fmt, args);
    va_end(args);
    font__draw(fnt, pos, text);
}

void font__push_state(const rizz_font* fnt)
{
    const font__fons* fons = (const font__fons*)fnt;
    fonsPushState(fons->ctx);
}

void font__pop_state(const rizz_font* fnt)
{
    const font__fons* fons = (const font__fons*)fnt;
    fonsPopState(fons->ctx);
}

void font__clear_state(const rizz_font* fnt)
{
    const font__fons* fons = (const font__fons*)fnt;
    fonsClearState(fons->ctx);
}

rizz_font_bounds font__bounds(const rizz_font* fnt, sx_vec2 pos, const char* text)
{
    const font__fons* fons = (const font__fons*)fnt;
    float bounds[4] = { 0 };
    float advance = fonsTextBounds(fons->ctx, pos.x, pos.y, text, NULL, bounds);
    return (rizz_font_bounds){ .rect = sx_rectf(bounds[0], bounds[1], bounds[2], bounds[3]),
                               .advance = advance };
}

rizz_font_line_bounds font__line_bounds(const rizz_font* fnt, float y)
{
    const font__fons* fons = (const font__fons*)fnt;
    float miny = 0, maxy = 0;
    fonsLineBounds(fons->ctx, y, &miny, &maxy);
    return (rizz_font_line_bounds){ .miny = miny, .maxy = maxy };
}

rizz_font_vert_metrics font__vert_metrics(const rizz_font* fnt)
{
    const font__fons* fons = (const font__fons*)fnt;
    float ascender = 0, descender = 0, lineh = 0;
    fonsVertMetrics(fons->ctx, &ascender, &descender, &lineh);
    return (rizz_font_vert_metrics){ .ascender = ascender, .descender = descender, .lineh = lineh };
}

rizz_font_iter font__iter_init(const rizz_font* fnt, sx_vec2 pos, const char* text)
{
    const font__fons* fons = (const font__fons*)fnt;
    FONStextIter fiter;
    if (fonsTextIterInit(fons->ctx, &fiter, pos.x, pos.y, text, NULL)) {
        return (rizz_font_iter){ .x = fiter.x,
                                 .y = fiter.y,
                                 .nextx = fiter.nextx,
                                 .nexty = fiter.nexty,
                                 .scale = fiter.scale,
                                 .spacing = fiter.spacing,
                                 .codepoint = fiter.codepoint,
                                 .isize = fiter.isize,
                                 .iblur = fiter.iblur,
                                 .prevGlyphIdx = fiter.prevGlyphIndex,
                                 .utf8state = fiter.utf8state,
                                 .str = fiter.str,
                                 .next = fiter.next,
                                 .end = fiter.end,
                                 ._reserved = fiter.font };
    } else {
        return (rizz_font_iter){ 0 };
    }
}

bool font__iter_next(const rizz_font* fnt, rizz_font_iter* iter, rizz_font_quad* quad)
{
    sx_assert(iter);

    const font__fons* fons = (const font__fons*)fnt;
    FONStextIter fiter = (FONStextIter){ .x = iter->x,
                                         .y = iter->y,
                                         .nextx = iter->nextx,
                                         .nexty = iter->nexty,
                                         .scale = iter->scale,
                                         .spacing = iter->spacing,
                                         .codepoint = iter->codepoint,
                                         .isize = iter->isize,
                                         .iblur = iter->iblur,
                                         .font = iter->_reserved,
                                         .prevGlyphIndex = iter->prevGlyphIdx,
                                         .str = iter->str,
                                         .next = iter->next,
                                         .end = iter->end,
                                         .utf8state = iter->utf8state };

    FONSquad fquad = { 0 };
    bool r = fonsTextIterNext(fons->ctx, &fiter, &fquad) ? true : false;

    *iter = (rizz_font_iter){ .x = fiter.x,
                              .y = fiter.y,
                              .nextx = fiter.nextx,
                              .nexty = fiter.nexty,
                              .scale = fiter.scale,
                              .spacing = fiter.spacing,
                              .codepoint = fiter.codepoint,
                              .isize = fiter.isize,
                              .iblur = fiter.iblur,
                              .prevGlyphIdx = fiter.prevGlyphIndex,
                              .utf8state = fiter.utf8state,
                              .str = fiter.str,
                              .next = fiter.next,
                              .end = fiter.end,
                              ._reserved = fiter.font };

    *quad =
        (rizz_font_quad){ .v0 = { .pos = {{ fquad.x0, fquad.y0 }}, .uv = {{ fquad.s0, fquad.t0 }} },
                          .v1 = { .pos = {{ fquad.x1, fquad.y1 }}, .uv = {{ fquad.s1, fquad.t1 }} } };

    return r;
}

void font__draw_debug(const rizz_font* fnt, sx_vec2 pos)
{
    const font__fons* fons = (const font__fons*)fnt;
    fonsDrawDebug(fons->ctx, pos.x, pos.y);
}

void font__set_size(const rizz_font* fnt, float size)
{
    const font__fons* fons = (const font__fons*)fnt;
    fonsSetSize(fons->ctx, size * (fons->ignore_dpiscale ? 1.0f : the_app->dpiscale()));
}

void font__set_color(const rizz_font* fnt, sx_color color)
{
    const font__fons* fons = (const font__fons*)fnt;
    fonsSetColor(fons->ctx, color.n);
}

void font__set_align(const rizz_font* fnt, rizz_font_align align_bits)
{
    const font__fons* fons = (const font__fons*)fnt;
    fonsSetAlign(fons->ctx, align_bits);
}

void font__set_spacing(const rizz_font* fnt, float spacing)
{
    const font__fons* fons = (const font__fons*)fnt;
    fonsSetSpacing(fons->ctx, spacing);
}

void font__set_blur(const rizz_font* fnt, float blur)
{
    const font__fons* fons = (const font__fons*)fnt;
    fonsSetBlur(fons->ctx, blur);
}

void font__set_scissor(const rizz_font* fnt, int x, int y, int width, int height)
{
    const font__fons* fons = (const font__fons*)fnt;
    fonsSetScissor(fons->ctx, x, y, width, height);
}

void font__set_viewproj_mat(const rizz_font* fnt, const sx_mat4* vp)
{
    const font__fons* fons = (const font__fons*)fnt;
    fonsSetMatrix(fons->ctx, vp->f);
}

void font__set_draw_api(rizz_api_gfx_draw* api)
{
    sx_assert(api);
    g_font.draw_api = api;
}