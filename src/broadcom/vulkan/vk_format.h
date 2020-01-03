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
#include <vulkan/vulkan.h>
#include <util/macros.h>
#include <vulkan/util/vk_format.h>


enum vk_format_colorspace {
   VK_FORMAT_COLORSPACE_RGB = 0,
   VK_FORMAT_COLORSPACE_SRGB = 1,
   VK_FORMAT_COLORSPACE_YUV = 2,
   VK_FORMAT_COLORSPACE_ZS = 3
};

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

#endif /* VK_FORMAT_H */
