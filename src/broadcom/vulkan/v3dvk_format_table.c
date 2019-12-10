/*
 * Copyright © 2014-2018 Broadcom
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include "util/format/u_format.h"
#include "v3d_cl.h"
#include "cle/v3dx_pack.h"
#include "common/v3d_macros.h"
#include "v3dvk_format.h"
#include "v3dvk_format_table.h"

#define SWIZ(x,y,z,w) {  \
        .r = VK_COMPONENT_SWIZZLE_##x, \
        .g = VK_COMPONENT_SWIZZLE_##y, \
        .b = VK_COMPONENT_SWIZZLE_##z, \
        .a = VK_COMPONENT_SWIZZLE_##w, \
}

#define FORMAT(pipe, rt, tex, swiz, return_size, return_channels) \
        [VK_FORMAT_##pipe] = {                                    \
                V3D_OUTPUT_IMAGE_FORMAT_##rt,                     \
                TEXTURE_DATA_FORMAT_##tex,                        \
                swiz,                                             \
                return_size,                                      \
                return_channels,                                  \
        }

#define SWIZ_X001	SWIZ(   R, ZERO, ZERO, ONE)
#define SWIZ_XY01	SWIZ(   R,    G, ZERO, ONE)
#define SWIZ_XYZ1	SWIZ(   R,    G,    B, ONE)
#define SWIZ_XYZW	SWIZ(   R,    G,    B,   A)
#define SWIZ_YZWX	SWIZ(   G,    B,    W,   R)
#define SWIZ_YZW1	SWIZ(   G,    B,    W, ONE)
#define SWIZ_ZYXW	SWIZ(   B,    G,    R,   A)
#define SWIZ_ZYX1	SWIZ(   B,    G,    R, ONE)
#define SWIZ_XXXY	SWIZ(   R,    R,    R,   G)
#define SWIZ_XXX1	SWIZ(   R,    R,    R, ONE)
#define SWIZ_XXXX	SWIZ(   R,    R,    R,   R)
#define SWIZ_000X	SWIZ(ZERO, ZERO, ZERO,   R)

static const struct v3dvk_format format_table[] = {
        FORMAT(B8G8R8A8_UNORM,    RGBA8,        RGBA8,       SWIZ_ZYXW, 16, 0),
        FORMAT(B8G8R8A8_SRGB,     SRGB8_ALPHA8, RGBA8,       SWIZ_ZYXW, 16, 0),
        FORMAT(R8G8B8A8_UNORM,    RGBA8,        RGBA8,       SWIZ_XYZW, 16, 0),
        FORMAT(R8G8B8A8_SRGB,     SRGB8_ALPHA8, RGBA8,       SWIZ_XYZW, 16, 0),
        FORMAT(A2R10G10B10_UNORM_PACK32, RGB10_A2, RGB10_A2, SWIZ_XYZW, 16, 0),
        FORMAT(A2R10G10B10_UINT_PACK32,  RGB10_A2UI, RGB10_A2UI, SWIZ_XYZW, 16, 0),

        FORMAT(R4G4B4A4_UNORM_PACK16, ABGR4444, RGBA4,       SWIZ_XYZW, 16, 0),

        FORMAT(A1R5G5B5_UNORM_PACK16, ABGR1555, RGB5_A1,     SWIZ_XYZW, 16, 0),
        FORMAT(B5G6R5_UNORM_PACK16, BGR565,     RGB565,      SWIZ_XYZ1, 16, 0),

        FORMAT(R8_UNORM,          R8,           R8,          SWIZ_X001, 16, 0),
        FORMAT(R8G8_UNORM,        RG8,          RG8,         SWIZ_XY01, 16, 0),

        FORMAT(R16_SFLOAT,        R16F,         R16F,        SWIZ_X001, 16, 0),
        FORMAT(R32_SFLOAT,        R32F,         R32F,        SWIZ_X001, 32, 1),

        FORMAT(R16G16_SFLOAT,     RG16F,        RG16F,       SWIZ_XY01, 16, 0),
        FORMAT(R32G32_SFLOAT,     RG32F,        RG32F,       SWIZ_XY01, 32, 2),

        FORMAT(R16G16B16A16_SFLOAT, RGBA16F,    RGBA16F,     SWIZ_XYZW, 16, 0),
        FORMAT(R32G32B32A32_SFLOAT, RGBA32F,    RGBA32F,     SWIZ_XYZW, 32, 4),

        FORMAT(R8_SINT,           R8I,          R8I,         SWIZ_X001, 16, 0),
        FORMAT(R8_UINT,           R8UI,         R8UI,        SWIZ_X001, 16, 0),
        FORMAT(R8G8_SINT,         RG8I,         RG8I,        SWIZ_XY01, 16, 0),
        FORMAT(R8G8_UINT,         RG8UI,        RG8UI,       SWIZ_XY01, 16, 0),
        FORMAT(R8G8B8A8_SINT,     RGBA8I,       RGBA8I,      SWIZ_XYZW, 16, 0),
        FORMAT(R8G8B8A8_UINT,     RGBA8UI,      RGBA8UI,     SWIZ_XYZW, 16, 0),

        FORMAT(R16_SINT,          R16I,         R16I,        SWIZ_X001, 16, 0),
        FORMAT(R16_UINT,          R16UI,        R16UI,       SWIZ_X001, 16, 0),
        FORMAT(R16G16_SINT,       RG16I,        RG16I,       SWIZ_XY01, 16, 0),
        FORMAT(R16G16_UINT,       RG16UI,       RG16UI,      SWIZ_XY01, 16, 0),
        FORMAT(R16G16B16A16_SINT, RGBA16I,      RGBA16I,     SWIZ_XYZW, 16, 0),
        FORMAT(R16G16B16A16_UINT, RGBA16UI,     RGBA16UI,    SWIZ_XYZW, 16, 0),

        FORMAT(R32_SINT,          R32I,         R32I,        SWIZ_X001, 32, 1),
        FORMAT(R32_UINT,          R32UI,        R32UI,       SWIZ_X001, 32, 1),
        FORMAT(R32G32_SINT,       RG32I,        RG32I,       SWIZ_XY01, 32, 2),
        FORMAT(R32G32_UINT,       RG32UI,       RG32UI,      SWIZ_XY01, 32, 2),
        FORMAT(R32G32B32A32_SINT, RGBA32I,      RGBA32I,     SWIZ_XYZW, 32, 4),
        FORMAT(R32G32B32A32_UINT, RGBA32UI,     RGBA32UI,    SWIZ_XYZW, 32, 4),

        FORMAT(R8_SINT,           R8I,          R8I,         SWIZ_000X, 16, 0),
        FORMAT(R8_UINT,           R8UI,         R8UI,        SWIZ_000X, 16, 0),
        FORMAT(R16_SINT,          R16I,         R16I,        SWIZ_000X, 16, 0),
        FORMAT(R16_UINT,          R16UI,        R16UI,       SWIZ_000X, 16, 0),
        FORMAT(R32_SINT,          R32I,         R32I,        SWIZ_000X, 32, 1),
        FORMAT(R32_UINT,          R32UI,        R32UI,       SWIZ_000X, 32, 1),

        FORMAT(B10G11R11_UFLOAT_PACK32, R11F_G11F_B10F, R11F_G11F_B10F, SWIZ_XYZW, 16, 0),

        FORMAT(D24_UNORM_S8_UINT, D24S8,        DEPTH24_X8,  SWIZ_XXXX, 32, 1),
        FORMAT(X8_D24_UNORM_PACK32, D24S8,        DEPTH24_X8,  SWIZ_XXXX, 32, 1),
        FORMAT(D24_UNORM_S8_UINT, S8,           RGBA8UI, SWIZ_XXXX, 16, 1),
        FORMAT(D32_SFLOAT,        D32F,         DEPTH_COMP32F, SWIZ_XXXX, 32, 1),
        FORMAT(D16_UNORM,         D16,          DEPTH_COMP16,SWIZ_XXXX, 32, 1),
};

const struct v3dvk_format *
v3d42_get_format_desc(VkFormat f)
{
        if (f < ARRAY_SIZE(format_table))
                return &format_table[f];
        else
                return NULL;
}

void
v3d42_get_internal_type_bpp_for_output_format(uint32_t format,
                                              uint32_t *type,
                                              uint32_t *bpp)
{
        switch (format) {
        case V3D_OUTPUT_IMAGE_FORMAT_RGBA8:
        case V3D_OUTPUT_IMAGE_FORMAT_RGB8:
        case V3D_OUTPUT_IMAGE_FORMAT_RG8:
        case V3D_OUTPUT_IMAGE_FORMAT_R8:
        case V3D_OUTPUT_IMAGE_FORMAT_ABGR4444:
        case V3D_OUTPUT_IMAGE_FORMAT_BGR565:
        case V3D_OUTPUT_IMAGE_FORMAT_ABGR1555:
                *type = V3D_INTERNAL_TYPE_8;
                *bpp = V3D_INTERNAL_BPP_32;
                break;

        case V3D_OUTPUT_IMAGE_FORMAT_RGBA8I:
        case V3D_OUTPUT_IMAGE_FORMAT_RG8I:
        case V3D_OUTPUT_IMAGE_FORMAT_R8I:
                *type = V3D_INTERNAL_TYPE_8I;
                *bpp = V3D_INTERNAL_BPP_32;
                break;

        case V3D_OUTPUT_IMAGE_FORMAT_RGBA8UI:
        case V3D_OUTPUT_IMAGE_FORMAT_RG8UI:
        case V3D_OUTPUT_IMAGE_FORMAT_R8UI:
                *type = V3D_INTERNAL_TYPE_8UI;
                *bpp = V3D_INTERNAL_BPP_32;
                break;

        case V3D_OUTPUT_IMAGE_FORMAT_SRGB8_ALPHA8:
        case V3D_OUTPUT_IMAGE_FORMAT_SRGB:
        case V3D_OUTPUT_IMAGE_FORMAT_RGB10_A2:
        case V3D_OUTPUT_IMAGE_FORMAT_R11F_G11F_B10F:
        case V3D_OUTPUT_IMAGE_FORMAT_RGBA16F:
                /* Note that sRGB RTs are stored in the tile buffer at 16F,
                 * and the conversion to sRGB happens at tilebuffer
                 * load/store.
                 */
                *type = V3D_INTERNAL_TYPE_16F;
                *bpp = V3D_INTERNAL_BPP_64;
                break;

        case V3D_OUTPUT_IMAGE_FORMAT_RG16F:
        case V3D_OUTPUT_IMAGE_FORMAT_R16F:
                *type = V3D_INTERNAL_TYPE_16F;
                /* Use 64bpp to make sure the TLB doesn't throw away the alpha
                 * channel before alpha test happens.
                 */
                *bpp = V3D_INTERNAL_BPP_64;
                break;

        case V3D_OUTPUT_IMAGE_FORMAT_RGBA16I:
                *type = V3D_INTERNAL_TYPE_16I;
                *bpp = V3D_INTERNAL_BPP_64;
                break;
        case V3D_OUTPUT_IMAGE_FORMAT_RG16I:
        case V3D_OUTPUT_IMAGE_FORMAT_R16I:
                *type = V3D_INTERNAL_TYPE_16I;
                *bpp = V3D_INTERNAL_BPP_32;
                break;

        case V3D_OUTPUT_IMAGE_FORMAT_RGB10_A2UI:
        case V3D_OUTPUT_IMAGE_FORMAT_RGBA16UI:
                *type = V3D_INTERNAL_TYPE_16UI;
                *bpp = V3D_INTERNAL_BPP_64;
                break;
        case V3D_OUTPUT_IMAGE_FORMAT_RG16UI:
        case V3D_OUTPUT_IMAGE_FORMAT_R16UI:
                *type = V3D_INTERNAL_TYPE_16UI;
                *bpp = V3D_INTERNAL_BPP_32;
                break;

        case V3D_OUTPUT_IMAGE_FORMAT_RGBA32I:
                *type = V3D_INTERNAL_TYPE_32I;
                *bpp = V3D_INTERNAL_BPP_128;
                break;
        case V3D_OUTPUT_IMAGE_FORMAT_RG32I:
                *type = V3D_INTERNAL_TYPE_32I;
                *bpp = V3D_INTERNAL_BPP_64;
                break;
        case V3D_OUTPUT_IMAGE_FORMAT_R32I:
                *type = V3D_INTERNAL_TYPE_32I;
                *bpp = V3D_INTERNAL_BPP_32;
                break;

        case V3D_OUTPUT_IMAGE_FORMAT_RGBA32UI:
                *type = V3D_INTERNAL_TYPE_32UI;
                *bpp = V3D_INTERNAL_BPP_128;
                break;
        case V3D_OUTPUT_IMAGE_FORMAT_RG32UI:
                *type = V3D_INTERNAL_TYPE_32UI;
                *bpp = V3D_INTERNAL_BPP_64;
                break;
        case V3D_OUTPUT_IMAGE_FORMAT_R32UI:
                *type = V3D_INTERNAL_TYPE_32UI;
                *bpp = V3D_INTERNAL_BPP_32;
                break;

        case V3D_OUTPUT_IMAGE_FORMAT_RGBA32F:
                *type = V3D_INTERNAL_TYPE_32F;
                *bpp = V3D_INTERNAL_BPP_128;
                break;
        case V3D_OUTPUT_IMAGE_FORMAT_RG32F:
                *type = V3D_INTERNAL_TYPE_32F;
                *bpp = V3D_INTERNAL_BPP_64;
                break;
        case V3D_OUTPUT_IMAGE_FORMAT_R32F:
                *type = V3D_INTERNAL_TYPE_32F;
                *bpp = V3D_INTERNAL_BPP_32;
                break;

        default:
                /* Provide some default values, as we'll be called at RB
                 * creation time, even if an RB with this format isn't
                 * supported.
                 */
                *type = V3D_INTERNAL_TYPE_8;
                *bpp = V3D_INTERNAL_BPP_32;
                break;
        }
}

bool
v3d42_tfu_supports_tex_format(enum V3D42_Texture_Data_Formats format)
{
        switch (format) {
        case TEXTURE_DATA_FORMAT_R8:
        case TEXTURE_DATA_FORMAT_R8_SNORM:
        case TEXTURE_DATA_FORMAT_RG8:
        case TEXTURE_DATA_FORMAT_RG8_SNORM:
        case TEXTURE_DATA_FORMAT_RGBA8:
        case TEXTURE_DATA_FORMAT_RGBA8_SNORM:
        case TEXTURE_DATA_FORMAT_RGB565:
        case TEXTURE_DATA_FORMAT_RGBA4:
        case TEXTURE_DATA_FORMAT_RGB5_A1:
        case TEXTURE_DATA_FORMAT_RGB10_A2:
        case TEXTURE_DATA_FORMAT_R16:
        case TEXTURE_DATA_FORMAT_R16_SNORM:
        case TEXTURE_DATA_FORMAT_RG16:
        case TEXTURE_DATA_FORMAT_RG16_SNORM:
        case TEXTURE_DATA_FORMAT_RGBA16:
        case TEXTURE_DATA_FORMAT_RGBA16_SNORM:
        case TEXTURE_DATA_FORMAT_R16F:
        case TEXTURE_DATA_FORMAT_RG16F:
        case TEXTURE_DATA_FORMAT_RGBA16F:
        case TEXTURE_DATA_FORMAT_R11F_G11F_B10F:
        case TEXTURE_DATA_FORMAT_R4:
                return true;
        default:
                return false;
        }
}
