/*
 * Copyright © 2015 Intel Corporation
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

#include "vulkan/util/vk_util.h"
#include "util/macros.h"
#include "common.h"
#include "v3dvk_entrypoints.h"
#include "v3dvk_formats.h"
#include "v3dvk_physical_device.h"

static const struct v3dvk_format main_formats[] = {
};

static const struct {
   const struct v3dvk_format *formats;
   uint32_t n_formats;
} v3dvk_formats[] = {
   [0]                                       = { .formats = main_formats,
                                                 .n_formats = ARRAY_SIZE(main_formats), },
#if 0
   [_VK_KHR_sampler_ycbcr_conversion_number] = { .formats = ycbcr_formats,
                                                 .n_formats = ARRAY_SIZE(ycbcr_formats), },
#endif
};

const struct v3dvk_format *
v3dvk_get_format(VkFormat vk_format)
{
   uint32_t enum_offset = VK_ENUM_OFFSET(vk_format);
   uint32_t ext_number = VK_ENUM_EXTENSION(vk_format);

   if (ext_number >= ARRAY_SIZE(v3dvk_formats) ||
       enum_offset >= v3dvk_formats[ext_number].n_formats)
      return NULL;

   const struct v3dvk_format *format =
      &v3dvk_formats[ext_number].formats[enum_offset];
#if 0
   if (format->planes[0].isl_format == ISL_FORMAT_UNSUPPORTED)
      return NULL;
#endif
   return format;
}

void v3dvk_GetPhysicalDeviceFormatProperties(
    VkPhysicalDevice                            physicalDevice,
    VkFormat                                    vk_format,
    VkFormatProperties*                         pFormatProperties)
{
   V3DVK_FROM_HANDLE(v3dvk_physical_device, physical_device, physicalDevice);
   const struct v3d_device_info *devinfo = &physical_device->info;
   const struct v3dvk_format *v3dvk_format = v3dvk_get_format(vk_format);

   *pFormatProperties = (VkFormatProperties) {
      .linearTilingFeatures = 0,
#if 0
         v3dvk_get_image_format_features(devinfo, vk_format, v3dvk_format,
                                       VK_IMAGE_TILING_LINEAR),
#endif
      .optimalTilingFeatures = 0,
#if 0
         v3dvk_get_image_format_features(devinfo, vk_format, v3dvk_format,
                                       VK_IMAGE_TILING_OPTIMAL),
#endif
      .bufferFeatures = 0,
#if 0
         get_buffer_format_features(devinfo, vk_format, v3dvk_format),
#endif
   };
}
