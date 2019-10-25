#pragma once

#include "sx/sx.h"

typedef struct rizz_texture_info rizz_texture_info;

typedef enum basisut_decode_flags {
    // PVRTC1: decode non-pow2 ETC1S texture level to the next larger power of 2 (not implemented
    // yet, but we're going to support it). Ignored if the slice's dimensions are already a power
    // of 2.
    cDecodeFlagsPVRTCDecodeToNextPow2 = 2,

    // When decoding to an opaque texture format, if the basis file has alpha, decode the alpha
    // slice instead of the color slice to the output texture format. This is primarily to allow
    // decoding of textures with alpha to multiple ETC1 textures (one for color, another for alpha).
    cDecodeFlagsTranscodeAlphaDataToOpaqueFormats = 4,

    // Forbid usage of BC1 3 color blocks (we don't support BC1 punchthrough alpha yet).
    // This flag is used internally when decoding to BC3.
    cDecodeFlagsBC1ForbidThreeColorBlocks = 8,

    // The output buffer contains alpha endpoint/selector indices.
    // Used internally when decoding formats like ASTC that require both color and alpha data to be
    // available when transcoding to the output format.
    cDecodeFlagsOutputHasAlphaIndices = 16
} basisut_decode_flags;

// clang-format off
// Compressed formats
typedef enum basisut_transcoder_texture_format {
	// ETC1-2
	cTFETC1_RGB = 0,							// Opaque only, returns RGB or alpha data if cDecodeFlagsTranscodeAlphaDataToOpaqueFormats flag is specified
	cTFETC2_RGBA = 1,							// Opaque+alpha, ETC2_EAC_A8 block followed by a ETC1 block, alpha channel will be opaque for opaque .basis files

	// BC1-5, BC7 (desktop, some mobile devices)
	cTFBC1_RGB = 2,							// Opaque only, no punchthrough alpha support yet, transcodes alpha slice if cDecodeFlagsTranscodeAlphaDataToOpaqueFormats flag is specified
	cTFBC3_RGBA = 3, 							// Opaque+alpha, BC4 followed by a BC1 block, alpha channel will be opaque for opaque .basis files
	cTFBC4_R = 4,								// Red only, alpha slice is transcoded to output if cDecodeFlagsTranscodeAlphaDataToOpaqueFormats flag is specified
	cTFBC5_RG = 5,								// XY: Two BC4 blocks, X=R and Y=Alpha, .basis file should have alpha data (if not Y will be all 255's)
	cTFBC7_M6_RGB = 6,						// Opaque only, RGB or alpha if cDecodeFlagsTranscodeAlphaDataToOpaqueFormats flag is specified. Highest quality of all the non-ETC1 formats.
	cTFBC7_M5_RGBA = 7,						// Opaque+alpha, alpha channel will be opaque for opaque .basis files

	// PVRTC1 4bpp (mobile, PowerVR devices)
	cTFPVRTC1_4_RGB = 8,						// Opaque only, RGB or alpha if cDecodeFlagsTranscodeAlphaDataToOpaqueFormats flag is specified, nearly lowest quality of any texture format.
	cTFPVRTC1_4_RGBA = 9,					// Opaque+alpha, most useful for simple opacity maps. If .basis file doens't have alpha cTFPVRTC1_4_RGB will be used instead. Lowest quality of any supported texture format.

	// ASTC (mobile, Intel devices, hopefully all desktop GPU's one day)
	cTFASTC_4x4_RGBA = 10,					// Opaque+alpha, ASTC 4x4, alpha channel will be opaque for opaque .basis files. Transcoder uses RGB/RGBA/L/LA modes, void extent, and up to two ([0,47] and [0,255]) endpoint precisions.

	// ATC (mobile, Adreno devices, this is a niche format)
	cTFATC_RGB = 11,							// Opaque, RGB or alpha if cDecodeFlagsTranscodeAlphaDataToOpaqueFormats flag is specified. ATI ATC (GL_ATC_RGB_AMD)
	cTFATC_RGBA = 12,							// Opaque+alpha, alpha channel will be opaque for opaque .basis files. ATI ATC (GL_ATC_RGBA_INTERPOLATED_ALPHA_AMD) 

	// FXT1 (desktop, Intel devices, this is a super obscure format)
	cTFFXT1_RGB = 17,							// Opaque only, uses exclusively CC_MIXED blocks. Notable for having a 8x4 block size. GL_3DFX_texture_compression_FXT1 is supported on Intel integrated GPU's (such as HD 630).
													// Punch-through alpha is relatively easy to support, but full alpha is harder. This format is only here for completeness so opaque-only is fine for now.
													// See the BASISU_USE_ORIGINAL_3DFX_FXT1_ENCODING macro in basisu_transcoder_internal.h.

	cTFPVRTC2_4_RGB = 18,					// Opaque-only, almost BC1 quality, much faster to transcode and supports arbitrary texture dimensions (unlike PVRTC1 RGB).
	cTFPVRTC2_4_RGBA = 19,					// Opaque+alpha, slower to encode than cTFPVRTC2_4_RGB. Premultiplied alpha is highly recommended, otherwise the color channel can leak into the alpha channel on transparent blocks.

	cTFETC2_EAC_R11 = 20,					// R only (ETC2 EAC R11 unsigned)
	cTFETC2_EAC_RG11 = 21,					// RG only (ETC2 EAC RG11 unsigned), R=opaque.r, G=alpha - for tangent space normal maps
		
	// Uncompressed (raw pixel) formats
	cTFRGBA32 = 13,							// 32bpp RGBA image stored in raster (not block) order in memory, R is first byte, A is last byte.
	cTFRGB565 = 14,							// 166pp RGB image stored in raster (not block) order in memory, R at bit position 11
	cTFBGR565 = 15,							// 16bpp RGB image stored in raster (not block) order in memory, R at bit position 0
	cTFRGBA4444 = 16,							// 16bpp RGBA image stored in raster (not block) order in memory, R at bit position 12, A at bit position 0

	cTFTotalTextureFormats = 22,

	// Old enums for compatibility with code compiled against previous versions
	cTFETC1 = cTFETC1_RGB,
	cTFETC2 = cTFETC2_RGBA,
	cTFBC1 = cTFBC1_RGB,
	cTFBC3 = cTFBC3_RGBA,
	cTFBC4 = cTFBC4_R,
	cTFBC5 = cTFBC5_RG,
	cTFBC7_M6_OPAQUE_ONLY = cTFBC7_M6_RGB,
	cTFBC7_M5 = cTFBC7_M5_RGBA,
	cTFASTC_4x4 = cTFASTC_4x4_RGBA,
	cTFATC_RGBA_INTERPOLATED_ALPHA = cTFATC_RGBA,
} basisut_transcoder_texture_format;
// clang-format on

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sx_alloc sx_alloc;
void basisut_init(const sx_alloc* alloc);
void basisut_release();

size_t basisut_transcoder_bytesize();
void* basisut_start_transcoding(void* memptr, const void* data, uint32_t data_size);
bool basisut_validate_header(const void* data, uint32_t data_size);
bool basisut_image_info(const void* data, uint32_t data_size, rizz_texture_info* info);
bool basisut_transcode_image_level(void* transcoder, const void* data, uint32_t data_size,
                                   uint32_t image_index, uint32_t level_index, void* pOutput_blocks,
                                   uint32_t output_blocks_buf_size_in_blocks_or_pixels,
                                   basisut_transcoder_texture_format fmt, uint32_t decode_flags);
uint32_t basisut_get_bytes_per_block(basisut_transcoder_texture_format fmt);
bool basisut_format_is_uncompressed(basisut_transcoder_texture_format fmt);
int basisut_get_uncompressed_bytes_per_pixel(basisut_transcoder_texture_format fmt);

#ifdef __cplusplus
}
#endif
