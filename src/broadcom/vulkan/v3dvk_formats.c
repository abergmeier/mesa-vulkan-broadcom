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
#include "broadcom/common/v3d_limits.h"
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
#if 0
   const struct v3dvk_format *v3dvk_format = v3dvk_get_format(vk_format);
#endif
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

static VkResult
v3dvk_get_image_format_properties(
   struct v3dvk_physical_device *physical_device,
   const VkPhysicalDeviceImageFormatInfo2 *info,
   VkImageFormatProperties *pImageFormatProperties,
   VkSamplerYcbcrConversionImageFormatProperties *pYcbcrImageFormatProperties)
{
   VkFormatFeatureFlags format_feature_flags;
   VkExtent3D maxExtent;
   uint32_t maxMipLevels;
   uint32_t maxArraySize;
   VkSampleCountFlags sampleCounts = VK_SAMPLE_COUNT_1_BIT;
   const struct v3d_device_info *devinfo = &physical_device->info;
   const struct v3dvk_format *format = v3dvk_get_format(info->format);

   if (format == NULL)
      goto unsupported;

   assert(format->vk_format == info->format);
#if 0
   format_feature_flags = anv_get_image_format_features(devinfo, info->format,
                                                        format, info->tiling);
#endif
   switch (info->type) {
   default:
      unreachable("bad VkImageType");
   case VK_IMAGE_TYPE_1D:
      maxExtent.width = 16384; // Only 14bit seem usable
      maxExtent.height = 1;
      maxExtent.depth = 1;
      maxMipLevels = V3D_MAX_MIP_LEVELS;
#if 0
      maxArraySize = 2048;
      sampleCounts = VK_SAMPLE_COUNT_1_BIT;
#endif
      break;
   case VK_IMAGE_TYPE_2D:
      /* FINISHME: Does this really differ for cube maps? The documentation
       * for RENDER_SURFACE_STATE suggests so.
       */
      maxExtent.width = 16384;
      maxExtent.height = 16384;
      maxExtent.depth = 1;
      maxMipLevels = V3D_MAX_MIP_LEVELS;
#if 0
      maxArraySize = 2048;
#endif
      break;
   case VK_IMAGE_TYPE_3D:
#if 0
      maxExtent.width = 2048;
      maxExtent.height = 2048;
      maxExtent.depth = 2048;
#endif
      maxMipLevels = V3D_MAX_MIP_LEVELS;
#if 0
      maxArraySize = 1;
#endif
      break;
   }
#if 0
   /* Our hardware doesn't support 1D compressed textures.
    *    From the SKL PRM, RENDER_SURFACE_STATE::SurfaceFormat:
    *    * This field cannot be a compressed (BC*, DXT*, FXT*, ETC*, EAC*) format
    *       if the Surface Type is SURFTYPE_1D.
    *    * This field cannot be ASTC format if the Surface Type is SURFTYPE_1D.
    */
   if (info->type == VK_IMAGE_TYPE_1D &&
       isl_format_is_compressed(format->planes[0].isl_format)) {
       goto unsupported;
   }

   if (info->tiling == VK_IMAGE_TILING_OPTIMAL &&
       info->type == VK_IMAGE_TYPE_2D &&
       (format_feature_flags & (VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT |
                                VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)) &&
       !(info->flags & VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT) &&
       !(info->usage & VK_IMAGE_USAGE_STORAGE_BIT)) {
      sampleCounts = isl_device_get_sample_counts(&physical_device->isl_dev);
   }

   if (info->usage & (VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                      VK_IMAGE_USAGE_TRANSFER_DST_BIT)) {
      /* Accept transfers on anything we can sample from or renderer to. */
      if (!(format_feature_flags & (VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT |
                                    VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT |
                                    VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT))) {
         goto unsupported;
      }
   }

   if (info->usage & VK_IMAGE_USAGE_SAMPLED_BIT) {
      if (!(format_feature_flags & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT)) {
         goto unsupported;
      }
   }

   if (info->usage & VK_IMAGE_USAGE_STORAGE_BIT) {
      if (!(format_feature_flags & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT)) {
         goto unsupported;
      }
   }

   if (info->usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) {
      if (!(format_feature_flags & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT)) {
         goto unsupported;
      }
   }

   if (info->usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) {
      if (!(format_feature_flags & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)) {
         goto unsupported;
      }
   }

   if (info->usage & VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT) {
      /* Nothing to check. */
   }

   if (info->usage & VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT) {
      /* Ignore this flag because it was removed from the
       * provisional_I_20150910 header.
       */
   }

   /* From the bspec section entitled "Surface Layout and Tiling",
    * pre-gen9 has a 2 GB limitation of the size in bytes,
    * gen9 and gen10 have a 256 GB limitation and gen11+
    * has a 16 TB limitation.
    */
   uint64_t maxResourceSize = 0;
   if (devinfo->gen < 9)
      maxResourceSize = (uint64_t) 1 << 31;
   else if (devinfo->gen < 11)
      maxResourceSize = (uint64_t) 1 << 38;
   else
      maxResourceSize = (uint64_t) 1 << 44;

   *pImageFormatProperties = (VkImageFormatProperties) {
      .maxExtent = maxExtent,
      .maxMipLevels = maxMipLevels,
      .maxArrayLayers = maxArraySize,
      .sampleCounts = sampleCounts,

      /* FINISHME: Accurately calculate
       * VkImageFormatProperties::maxResourceSize.
       */
      .maxResourceSize = maxResourceSize,
   };

   if (pYcbcrImageFormatProperties) {
      pYcbcrImageFormatProperties->combinedImageSamplerDescriptorCount =
         format->n_planes;
   }

   return VK_SUCCESS;
#endif
unsupported:
   *pImageFormatProperties = (VkImageFormatProperties) {
      .maxExtent = { 0, 0, 0 },
      .maxMipLevels = 0,
      .maxArrayLayers = 0,
      .sampleCounts = 0,
      .maxResourceSize = 0,
   };

   return VK_ERROR_FORMAT_NOT_SUPPORTED;
}

VkResult v3dvk_GetPhysicalDeviceImageFormatProperties(
    VkPhysicalDevice                            physicalDevice,
    VkFormat                                    format,
    VkImageType                                 type,
    VkImageTiling                               tiling,
    VkImageUsageFlags                           usage,
    VkImageCreateFlags                          createFlags,
    VkImageFormatProperties*                    pImageFormatProperties)
{
   V3DVK_FROM_HANDLE(v3dvk_physical_device, physical_device, physicalDevice);

   const VkPhysicalDeviceImageFormatInfo2 info = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2,
      .pNext = NULL,
      .format = format,
      .type = type,
      .tiling = tiling,
      .usage = usage,
      .flags = createFlags,
   };

   return v3dvk_get_image_format_properties(physical_device, &info,
                                            pImageFormatProperties, NULL);
}

void v3dvk_GetPhysicalDeviceSparseImageFormatProperties(
    VkPhysicalDevice                            physicalDevice,
    VkFormat                                    format,
    VkImageType                                 type,
    uint32_t                                    samples,
    VkImageUsageFlags                           usage,
    VkImageTiling                               tiling,
    uint32_t*                                   pNumProperties,
    VkSparseImageFormatProperties*              pProperties)
{
   /* Sparse images are not yet supported. */
   *pNumProperties = 0;
}
