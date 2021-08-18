#include "rizz/rizz.h"

#include "sx/string.h"
#include "sx/os.h"

SX_PRAGMA_DIAGNOSTIC_PUSH()
SX_PRAGMA_DIAGNOSTIC_IGNORED_CLANG_GCC("-Wunused-parameter")
#include "basisu/basisu_transcoder.h"
SX_PRAGMA_DIAGNOSTIC_POP();

#define CHECKER_TEXTURE_SIZE        128

RIZZ_STATE static rizz_api_core* the_core;
RIZZ_STATE static rizz_api_gfx* the_gfx;
RIZZ_STATE static rizz_api_asset* the_asset;

static const sx_alloc* g_basisut_alloc;
static basist::etc1_global_selector_codebook* g_basisut_cookbook;
static basist::basisu_transcoder* g_basisut_transcoder;
static rizz_texture g_texture_checker;
static rizz_texture g_texture_blank;

typedef struct basisut_transcode_data {
    basist::transcoder_texture_format fmt;
    int mip_size[SG_MAX_MIPMAPS];
} basisut_transcode_data;

static rizz_asset_load_data basisut_on_prepare(const rizz_asset_load_params* params, const sx_mem_block* mem);
static bool basisut_on_load(rizz_asset_load_data* data, const rizz_asset_load_params* params, const sx_mem_block* mem);
static void basisut_on_finalize(rizz_asset_load_data* data, const rizz_asset_load_params* params, const sx_mem_block* mem);
static void basisut_on_reload(rizz_asset handle, rizz_asset_obj prev_obj, const sx_alloc* alloc);
static void basisut_on_release(rizz_asset_obj obj, const sx_alloc* alloc);

static void basisut_init(const sx_alloc* alloc)
{
    sx_assert(alloc);
    g_basisut_alloc = alloc;
    g_basisut_cookbook =
        new (sx_malloc(g_basisut_alloc, sizeof(basist::etc1_global_selector_codebook)))
            basist::etc1_global_selector_codebook(basist::g_global_selector_cb_size,
                                                  basist::g_global_selector_cb);
    sx_assert(g_basisut_cookbook);
    g_basisut_transcoder = new (sx_malloc(g_basisut_alloc, sizeof(basist::basisu_transcoder)))
        basist::basisu_transcoder(g_basisut_cookbook);
    sx_assert(g_basisut_transcoder);
    basist::basisu_transcoder_init();

    rizz_asset_callbacks callbacks;
    callbacks.on_prepare = basisut_on_prepare;
    callbacks.on_load = basisut_on_load;
    callbacks.on_finalize = basisut_on_finalize;
    callbacks.on_reload = basisut_on_reload;
    callbacks.on_release = basisut_on_release;
    
    const sx_color checker_colors[] = { sx_color4u(255, 0, 255, 255), sx_color4u(255, 255, 255, 255) };    
    g_texture_checker = the_gfx->texture_create_checker(CHECKER_TEXTURE_SIZE/2, CHECKER_TEXTURE_SIZE, checker_colors);
    g_texture_blank.img = the_gfx->texture_white();
    g_texture_blank.info.type = SG_IMAGETYPE_2D;
    g_texture_blank.info.format = SG_PIXELFORMAT_RGBA8;
    g_texture_blank.info.mem_size_bytes = sizeof(uint32_t);
    g_texture_blank.info.width = 1;
    g_texture_blank.info.height = 1;
    g_texture_blank.info.layers = 1;
    g_texture_blank.info.mips = 1;
    g_texture_blank.info.bpp = 32;

    rizz_asset_obj checker_asset;
    rizz_asset_obj blank_asset;
    checker_asset.ptr = &g_texture_checker;
    blank_asset.ptr = &g_texture_blank;

    the_asset->register_asset_type("texture_basisu",
                                   callbacks,
                                   "rizz_texture_load_params", sizeof(rizz_texture_load_params),
                                   checker_asset, blank_asset, 0);
}

static void basisut_release()
{
    sx_assert(g_basisut_alloc);

    g_basisut_transcoder->~basisu_transcoder();
    sx_free(g_basisut_alloc, g_basisut_transcoder);
    g_basisut_transcoder = NULL;

    g_basisut_cookbook->~etc1_global_selector_codebook();
    sx_free(g_basisut_alloc, g_basisut_cookbook);
}

static bool basisut_validate_header(const void* data, uint32_t data_size)
{
    sx_assert(g_basisut_transcoder);
    return g_basisut_transcoder->validate_header(data, data_size);
}

static bool basisut_image_info(const void* data, uint32_t data_size, rizz_texture_info* info)
{
    sx_assert(g_basisut_transcoder);
    basist::basisu_file_info file_info;
    g_basisut_transcoder->get_file_info(data, data_size, file_info);

    // clang-format off
    switch (file_info.m_tex_type) {
    case basist::cBASISTexType2D:           info->type = SG_IMAGETYPE_2D;       break;
    case basist::cBASISTexType2DArray:      info->type = SG_IMAGETYPE_ARRAY;    break;
    case basist::cBASISTexTypeCubemapArray: info->type = SG_IMAGETYPE_CUBE;     break;
    case basist::cBASISTexTypeVolume:       info->type = SG_IMAGETYPE_3D;       break;
    default:
        sx_assert_alwaysf(0, "this type of basis file is not supported");
        return false;
    }
    // clang-format on

    if (file_info.m_tex_type == basist::cBASISTexTypeCubemapArray &&
        (file_info.m_total_images % 6) != 1) {
        rizz_log_warn("cube arrays are not supported");
        sx_assert(0);
        file_info.m_total_images = 6;
    }

    sx_assert(file_info.m_total_images > 0);
    basist::basisu_image_info image_info;
    g_basisut_transcoder->get_image_info(data, data_size, image_info, 0);

    info->format = _SG_PIXELFORMAT_DEFAULT;
    info->width = (int)image_info.m_width;
    info->height = (int)image_info.m_height;
    info->layers = (int)file_info.m_total_images;
    info->mips = (int)image_info.m_total_levels;
    info->mem_size_bytes = data_size;
    info->bpp = 0;

    return true;
}

static void* basisut_start_transcoding(void* memptr, const void* data, uint32_t data_size)
{
    sx_assert(memptr);
    // create a new transcoder instance
    basist::basisu_transcoder* transcoder =
        new (memptr) basist::basisu_transcoder(g_basisut_cookbook);
    if (transcoder) {
        bool r = transcoder->start_transcoding(data, data_size);
        return r ? transcoder : NULL;
    } else {
        return NULL;
    }
}

static bool basisut_transcode_image_level(void* _trans, const void* data, uint32_t data_size,
                                          uint32_t image_index, uint32_t level_index, void* pOutput_blocks,
                                          uint32_t output_blocks_buf_size_in_blocks_or_pixels,
                                          basist::transcoder_texture_format fmt, uint32_t decode_flags)
{
    basist::basisu_transcoder* transcoder = reinterpret_cast<basist::basisu_transcoder*>(_trans);
    return transcoder->transcode_image_level(data, data_size, image_index, level_index,
                                             pOutput_blocks,
                                             output_blocks_buf_size_in_blocks_or_pixels,
                                             fmt, decode_flags);
}

static rizz_asset_load_data basisut_on_prepare(const rizz_asset_load_params* params, const sx_mem_block* mem)
{
    const sx_alloc* alloc = params->alloc ? params->alloc : g_basisut_alloc;

    rizz_texture* tex = (rizz_texture*)sx_calloc(alloc, sizeof(rizz_texture));
    if (!tex) {
        sx_out_of_memory();
        return { 0 };
    }

    rizz_texture_info* info = &tex->info;
    char ext[32];
    sx_os_path_ext(ext, sizeof(ext), params->path);

    sx_assert(sx_strequalnocase(ext, ".basis"));
    if (basisut_validate_header(mem->data, (uint32_t)mem->size)) {
        bool r = basisut_image_info(mem->data, (uint32_t)mem->size, info);
        sx_unused(r);
        sx_assert(r);
    } else {
        rizz_log_warn("reading texture '%s' metadata failed", params->path);
        sx_free(alloc, tex);
        return { 0 };
    }

    const rizz_texture_load_params* tparams = (const rizz_texture_load_params*)params->params;
    sx_assertf(tparams->fmt != _SG_PIXELFORMAT_DEFAULT, "fmt must be defined for basis files");

    // clang-format off
    basist::transcoder_texture_format basis_fmt;
    switch (tparams->fmt) {
    case SG_PIXELFORMAT_ETC2_RGB8:  basis_fmt = basist::transcoder_texture_format::cTFETC1; break;
    case SG_PIXELFORMAT_ETC2_RGBA8: basis_fmt = basist::transcoder_texture_format::cTFETC2; break;
    case SG_PIXELFORMAT_ETC2_RG11:  basis_fmt = basist::transcoder_texture_format::cTFETC2_EAC_RG11; break;
    case SG_PIXELFORMAT_BC1_RGBA:   basis_fmt = basist::transcoder_texture_format::cTFBC1; break;
    case SG_PIXELFORMAT_BC3_RGBA:   basis_fmt = basist::transcoder_texture_format::cTFBC3; break;
    case SG_PIXELFORMAT_BC4_R:      basis_fmt = basist::transcoder_texture_format::cTFBC4; break;
    case SG_PIXELFORMAT_BC5_RG:     basis_fmt = basist::transcoder_texture_format::cTFBC5; break;
    case SG_PIXELFORMAT_BC7_RGBA:   basis_fmt = basist::transcoder_texture_format::cTFBC7_M5;  break;
    case SG_PIXELFORMAT_PVRTC_RGBA_4BPP:    basis_fmt = basist::transcoder_texture_format::cTFPVRTC1_4_RGBA;   break;
    case SG_PIXELFORMAT_PVRTC_RGB_4BPP:     basis_fmt = basist::transcoder_texture_format::cTFPVRTC1_4_RGB;    break;
    case SG_PIXELFORMAT_RGBA8:              basis_fmt = basist::transcoder_texture_format::cTFRGBA32;          break;
    default:
        rizz_log_warn("parsing texture '%s' failed. transcoding of this format is not supported");
        sx_assert(0);
        return { 0 };
    }

    tex->info.format = tparams->fmt;
    int w = tex->info.width;
    int h = tex->info.height;
    int num_mips = tex->info.mips;
    int num_images = tex->info.layers;

    size_t total_sz = sizeof(sg_image_desc) + sizeof(basist::basisu_transcoder) + sizeof(basisut_transcode_data);
    int mip_size[SG_MAX_MIPMAPS];

    // calculate the buffer sizes needed for holding all the output pixels
    sx_assert(num_mips < SG_MAX_MIPMAPS);

    for (int i = 0; i < num_images; i++) {
        for (int mip = 0; mip < num_mips; mip++) {
            if (mip >= tparams->first_mip) {
                int image_sz = the_gfx->texture_surface_pitch(tparams->fmt, w, h, 1);
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

    uint8_t* buff = (uint8_t*)sx_malloc(g_basisut_alloc, total_sz);
    if (!buff) {
        sx_out_of_memory();
        return { 0 };
    }
    
    void* user_data = buff;
    buff += sizeof(sg_image_desc) + sizeof(basist::basisu_transcoder);
    basisut_transcode_data* transcode_data = (basisut_transcode_data*)buff;
    transcode_data->fmt = basis_fmt;
    sx_memcpy(transcode_data->mip_size, mip_size, sizeof(mip_size));

    tex->img = the_gfx->alloc_image();
    sx_assert(tex->img.id);

    rizz_asset_load_data ldata;
    ldata.obj.ptr = tex;
    ldata.user1 = user_data;

    return ldata;
}

static bool basisut_on_load(rizz_asset_load_data* data, const rizz_asset_load_params* params, const sx_mem_block* mem)
{
    auto tparams = (const rizz_texture_load_params*)params->params;
    auto tex = (rizz_texture*)data->obj.ptr;
    auto desc = (sg_image_desc*)data->user1;

    int default_first_mip, default_aniso;
    sg_filter default_min_filter, default_mag_filter;
    the_gfx->texture_default_quality(&default_min_filter, &default_mag_filter, &default_aniso, &default_first_mip);
    int first_mip = tparams->first_mip ? tparams->first_mip :  default_first_mip;
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
    sx_memset(desc, 0x0, sizeof(*desc));
    desc->type = tex->info.type,
    desc->width = tex->info.width,
    desc->height = tex->info.height,
    desc->num_slices = tex->info.layers,
    desc->num_mipmaps = num_mips, 
    desc->pixel_format = tex->info.format,
    desc->min_filter = tparams->min_filter != 0 ? tparams->min_filter : default_min_filter,
    desc->mag_filter = tparams->mag_filter != 0 ? tparams->mag_filter : default_mag_filter,
    desc->wrap_u = tparams->wrap_u,
    desc->wrap_v = tparams->wrap_v,
    desc->wrap_w = tparams->wrap_w,
    desc->max_anisotropy = tparams->aniso ? (uint32_t)tparams->aniso : (uint32_t)default_aniso,
    desc->srgb = tparams->srgb;

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

    sx_assert(sx_strequalnocase(ext, ".basis"));

    sx_assert(tparams->fmt != _SG_PIXELFORMAT_DEFAULT);
    if (tparams->fmt != _SG_PIXELFORMAT_DEFAULT) {
        auto transcoder_obj_buffer = (uint8_t*)(desc + 1);
        void* trans = basisut_start_transcoding(transcoder_obj_buffer, mem->data, (uint32_t)mem->size);
        sx_assert(trans);

        // we have extra buffers for this particular type of file
        auto transcode_data = (basisut_transcode_data*)(transcoder_obj_buffer + sizeof(basist::basisu_transcoder));
        auto transcode_buff = (uint8_t*)(transcode_data + 1);

        int num_images = tex->info.type == SG_IMAGETYPE_2D ? 1 : tex->info.layers;
        int bytes_per_block = basist::basis_transcoder_format_is_uncompressed(transcode_data->fmt) ? 
                basist::basis_get_uncompressed_bytes_per_pixel(transcode_data->fmt) : 
                (int)basist::basis_get_bytes_per_block(transcode_data->fmt);

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
        rizz_log_warn("parsing texture '%s' failed", params->path);
        return false;
    }

    return true;
}

static void basisut_on_finalize(rizz_asset_load_data* data, const rizz_asset_load_params* params, const sx_mem_block* mem)
{
    sx_unused(mem);

    auto tex = (rizz_texture*)data->obj.ptr;
    auto desc = (sg_image_desc*)data->user1;
    sx_assert(desc);

    char basename[64];
    desc->label = the_core->str_alloc(&tex->info.name_hdl, sx_os_path_basename(basename, sizeof(basename), params->path));

    the_gfx->init_image(tex->img, desc);
    sx_free(g_basisut_alloc, data->user1);
}

static void basisut_on_reload(rizz_asset handle, rizz_asset_obj prev_obj, const sx_alloc* alloc)
{
    sx_unused(prev_obj);
    sx_unused(handle);
    sx_unused(alloc);
}

static void basisut_on_release(rizz_asset_obj obj, const sx_alloc* alloc)
{
    auto tex = (rizz_texture*)obj.ptr;
    sx_assert(tex);

    if (!alloc)
        alloc = g_basisut_alloc;

    if (tex->img.id)
        the_gfx->destroy_image(tex->img);

    if (tex->info.name_hdl)
        the_core->str_free(tex->info.name_hdl);

    sx_free(alloc, tex);    
}

rizz_plugin_decl_main(basisut, plugin, e)
{
    switch (e) {
    case RIZZ_PLUGIN_EVENT_STEP: {
        break;
    }
    case RIZZ_PLUGIN_EVENT_INIT:
    {
        rizz_api_plugin* the_plugin = plugin->api; 
        the_core = (rizz_api_core*)the_plugin->get_api(RIZZ_API_CORE, 0);
        the_asset = (rizz_api_asset*)the_plugin->get_api(RIZZ_API_ASSET, 0);
        the_gfx = (rizz_api_gfx*)the_plugin->get_api(RIZZ_API_GFX, 0);

        basisut_init(the_gfx->alloc());
        break;
    }
    case RIZZ_PLUGIN_EVENT_LOAD:
        break;
    case RIZZ_PLUGIN_EVENT_UNLOAD:
        break;
    case RIZZ_PLUGIN_EVENT_SHUTDOWN:
        basisut_release();
        break; 
    }

    return 0;
}

rizz_plugin_decl_event_handler(basisut, e)
{
    sx_unused(e);
}

rizz_plugin_implement_info(basisut, 1000, "basisut texture plugin", NULL, 1);