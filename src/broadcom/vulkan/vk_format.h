/*
 * Copyright © 2016 Red Hat.
 * Copyright © 2016 Bas Nieuwenhuizen
 *
 * Based on u_format.h which is:
 * Copyright 2009-2010 Vmware, Inc.
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
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifndef VK_FORMAT_H
#define VK_FORMAT_H

#include <assert.h>
#include <stdbool.h>
#include <vulkan/vulkan.h>
#include <util/macros.h>
#include <vulkan/util/vk_format.h>


enum vk_format_colorspace {
   VK_FORMAT_COLORSPACE_RGB = 0,
   VK_FORMAT_COLORSPACE_SRGB = 1,
   VK_FORMAT_COLORSPACE_YUV = 2,
   VK_FORMAT_COLORSPACE_ZS = 3
};

struct vk_format_description {
        /** One of V3D33_OUTPUT_IMAGE_FORMAT_* */
        uint8_t rt_type;

        /** One of V3D33_TEXTURE_DATA_FORMAT_*. */
        uint8_t tex_type;

        /**
         * Swizzle to apply to the RGBA shader output for storing to the tile
         * buffer, to the RGBA tile buffer to produce shader input (for
         * blending), and for turning the rgba8888 texture sampler return
         * value into shader rgba values.
         */
        VkComponentMapping swizzle;

        /* Whether the return value is 16F/I/UI or 32F/I/UI. */
        uint8_t return_size;

        /* If return_size == 32, how many channels are returned by texturing.
         * 16 always returns 2 pairs of 16 bit values.
         */
        uint8_t return_channels;

        enum vk_format_colorspace colorspace;
};

const struct vk_format_description *
v3d41_get_format_desc(VkFormat f);

static inline VkFormat
vk_format_depth_only(VkFormat format)
{
   switch (format) {
   case VK_FORMAT_D16_UNORM_S8_UINT:
      return VK_FORMAT_D16_UNORM;
   case VK_FORMAT_D24_UNORM_S8_UINT:
      return VK_FORMAT_X8_D24_UNORM_PACK32;
   case VK_FORMAT_D32_SFLOAT_S8_UINT:
      return VK_FORMAT_D32_SFLOAT;
   default:
      return format;
   }
}

static inline VkFormat
vk_format_stencil_only(VkFormat format)
{
   return VK_FORMAT_S8_UINT;
}

static inline bool
vk_format_is_srgb(VkFormat format)
{
        const struct vk_format_description *desc = v3d41_get_format_desc(format);
        return desc->colorspace == VK_FORMAT_COLORSPACE_SRGB;
}

static inline const struct util_format_description *
vk_format_description(VkFormat format)
{
   return util_format_description(vk_format_to_pipe_format(format));
}

/**
 * Return total bits needed for the pixel format per block.
 */
static inline unsigned
vk_format_get_blocksizebits(VkFormat format)
{
#if 0
        const struct vk_format_description *desc = v3d41_get_format_desc(format);

        assert(desc);
        if (!desc) {
                return 0;
        }

        return desc->block.bits;
#endif
   return util_format_get_blocksizebits(vk_format_to_pipe_format(format));
}

/**
 * Return bytes per block (not pixel) for the given format.
 */
static inline unsigned
vk_format_get_blocksize(VkFormat format)
{
#if 0
        unsigned bits = vk_format_get_blocksizebits(format);
        unsigned bytes = bits / 8;

        assert(bits % 8 == 0);
        assert(bytes > 0);
        if (bytes == 0) {
                bytes = 1;
        }

        return bytes;
#endif
   return util_format_get_blocksize(vk_format_to_pipe_format(format));
}

static inline unsigned
vk_format_get_blockwidth(VkFormat format)
{
#if 0
        const struct vk_format_description *desc = v3d41_get_format_desc(format);

        assert(desc);
        if (!desc) {
                return 1;
        }

        return desc->block.width;
#endif
   return util_format_get_blockwidth(vk_format_to_pipe_format(format));
}

static inline unsigned
vk_format_get_blockheight(VkFormat format)
{
   return util_format_get_blockheight(vk_format_to_pipe_format(format));
}

#endif /* VK_FORMAT_H */
