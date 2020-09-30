#include "sx/allocator.h"

SX_PRAGMA_DIAGNOSTIC_PUSH()
SX_PRAGMA_DIAGNOSTIC_IGNORED_CLANG_GCC("-Wunused-parameter")
#include "basisu/basisu_transcoder.h"
SX_PRAGMA_DIAGNOSTIC_POP();

#include "internal.h"
#include "basisut.h"

static const sx_alloc* g_basisut_alloc;
static basist::etc1_global_selector_codebook* g_basisut_cookbook;
basist::basisu_transcoder* g_basisut_transcoder;

void basisut_init(const sx_alloc* alloc)
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
}

void basisut_release()
{
    sx_assert(g_basisut_alloc);

    g_basisut_transcoder->~basisu_transcoder();
    sx_free(g_basisut_alloc, g_basisut_transcoder);
    g_basisut_transcoder = NULL;

    g_basisut_cookbook->~etc1_global_selector_codebook();
    sx_free(g_basisut_alloc, g_basisut_cookbook);
}

size_t basisut_transcoder_bytesize()
{
    return sizeof(basist::basisu_transcoder);
}

bool basisut_validate_header(const void* data, uint32_t data_size)
{
    sx_assert(g_basisut_transcoder);
    return g_basisut_transcoder->validate_header(data, data_size);
}

bool basisut_image_info(const void* data, uint32_t data_size, rizz_texture_info* info)
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
        rizz__log_warn("cube arrays are not supported");
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

void* basisut_start_transcoding(void* memptr, const void* data, uint32_t data_size)
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

bool basisut_transcode_image_level(void* _trans, const void* data, uint32_t data_size,
                                   uint32_t image_index, uint32_t level_index, void* pOutput_blocks,
                                   uint32_t output_blocks_buf_size_in_blocks_or_pixels,
                                   basisut_transcoder_texture_format fmt, uint32_t decode_flags)
{
    basist::basisu_transcoder* transcoder = reinterpret_cast<basist::basisu_transcoder*>(_trans);
    return transcoder->transcode_image_level(data, data_size, image_index, level_index,
                                             pOutput_blocks,
                                             output_blocks_buf_size_in_blocks_or_pixels,
                                             (basist::transcoder_texture_format)fmt, decode_flags);
}

uint32_t basisut_get_bytes_per_block(basisut_transcoder_texture_format fmt)
{
    return basist::basis_get_bytes_per_block((basist::transcoder_texture_format)fmt);
}

bool basisut_format_is_uncompressed(basisut_transcoder_texture_format fmt)
{
    return basist::basis_transcoder_format_is_uncompressed((basist::transcoder_texture_format)fmt);
}

int basisut_get_uncompressed_bytes_per_pixel(basisut_transcoder_texture_format fmt)
{
    return basist::basis_get_uncompressed_bytes_per_pixel((basist::transcoder_texture_format)fmt);
}
