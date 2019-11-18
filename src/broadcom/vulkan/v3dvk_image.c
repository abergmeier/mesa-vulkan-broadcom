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

#include <stdlib.h>
#include <drm-uapi/drm_fourcc.h>

#include "vulkan/util/vk_alloc.h"
#include "vulkan/util/vk_util.h"
#include "vulkan/wsi/wsi_common.h"
#include "common.h"
#include "device.h"
#include "v3dvk_defines.h"
#include "v3dvk_error.h"
#include "v3dvk_formats.h"
#include "v3dvk_image.h"
#include "vk_format_info.h"

VkResult
v3dvk_image_create(VkDevice _device,
                   const struct v3dvk_image_create_info *create_info,
                   const VkAllocationCallbacks* alloc,
                   VkImage *pImage)
{
   V3DVK_FROM_HANDLE(v3dvk_device, device, _device);
   const VkImageCreateInfo *pCreateInfo = create_info->vk_info;
#if 0
   const struct isl_drm_modifier_info *isl_mod_info = NULL;
#endif
   struct v3dvk_image *image = NULL;
   VkResult r;

   assert(pCreateInfo->sType == VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO);

   const struct wsi_image_create_info *wsi_info =
      vk_find_struct_const(pCreateInfo->pNext, WSI_IMAGE_CREATE_INFO_MESA);
   if (wsi_info && wsi_info->modifier_count > 0) {
#if 0
      isl_mod_info = choose_drm_format_mod(&device->instance->physicalDevice,
                                           wsi_info->modifier_count,
                                           wsi_info->modifiers);
      assert(isl_mod_info);
#endif
   }

   v3dvk_assert(pCreateInfo->mipLevels > 0);
   v3dvk_assert(pCreateInfo->arrayLayers > 0);
   v3dvk_assert(pCreateInfo->samples > 0);
   v3dvk_assert(pCreateInfo->extent.width > 0);
   v3dvk_assert(pCreateInfo->extent.height > 0);
   v3dvk_assert(pCreateInfo->extent.depth > 0);

   image = vk_zalloc2(&device->alloc, alloc, sizeof(*image), 8,
                       VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
   if (!image)
      return v3dvk_error(device->instance, VK_ERROR_OUT_OF_HOST_MEMORY);

   image->type = pCreateInfo->imageType;
   image->extent = pCreateInfo->extent;
   image->vk_format = pCreateInfo->format;
   image->format = v3dvk_get_format(pCreateInfo->format);
   image->aspects = vk_format_aspects(image->vk_format);
   image->levels = pCreateInfo->mipLevels;
   image->array_size = pCreateInfo->arrayLayers;
   image->samples = pCreateInfo->samples;
   image->usage = pCreateInfo->usage;
   image->create_flags = pCreateInfo->flags;
   image->tiling = pCreateInfo->tiling;
   image->disjoint = pCreateInfo->flags & VK_IMAGE_CREATE_DISJOINT_BIT;
#if 0
   image->needs_set_tiling = wsi_info && wsi_info->scanout;
   image->drm_format_mod = isl_mod_info ? isl_mod_info->modifier :
                                          DRM_FORMAT_MOD_INVALID;
#endif
   if (image->aspects & VK_IMAGE_ASPECT_STENCIL_BIT) {
      image->stencil_usage = pCreateInfo->usage;
      const VkImageStencilUsageCreateInfoEXT *stencil_usage_info =
         vk_find_struct_const(pCreateInfo->pNext,
                              IMAGE_STENCIL_USAGE_CREATE_INFO_EXT);
      if (stencil_usage_info)
         image->stencil_usage = stencil_usage_info->stencilUsage;
   }

   /* In case of external format, We don't know format yet,
    * so skip the rest for now.
    */
   if (create_info->external_format) {
      image->external_format = true;
      *pImage = v3dvk_image_to_handle(image);
      return VK_SUCCESS;
   }

#if 0
   const struct v3dvk_format *format = v3dvk_get_format(image->vk_format);
   assert(format != NULL);
   const isl_tiling_flags_t isl_tiling_flags =
      choose_isl_tiling_flags(create_info, isl_mod_info,
                              image->needs_set_tiling);

   image->n_planes = format->n_planes;
#endif
   const VkImageFormatListCreateInfoKHR *fmt_list =
      vk_find_struct_const(pCreateInfo->pNext,
                           IMAGE_FORMAT_LIST_CREATE_INFO_KHR);
#if 0
   image->ccs_e_compatible =
      all_formats_ccs_e_compatible(&device->info, fmt_list, image);

   uint32_t b;
   for_each_bit(b, image->aspects) {
      r = make_surface(device, image, create_info->stride, isl_tiling_flags,
                       create_info->isl_extra_usage_flags, (1 << b));
      if (r != VK_SUCCESS)
         goto fail;
   }
#endif
   *pImage = v3dvk_image_to_handle(image);

   return VK_SUCCESS;

fail:
   if (image)
      vk_free2(&device->alloc, alloc, image);

   return r;
}

static struct v3dvk_image *
v3dvk_swapchain_get_image(VkSwapchainKHR swapchain,
                          uint32_t index)
{
   uint32_t n_images = index + 1;
   VkImage *images = malloc(sizeof(*images) * n_images);
   VkResult result = wsi_common_get_images(swapchain, &n_images, images);

   if (result != VK_SUCCESS && result != VK_INCOMPLETE) {
      free(images);
      return NULL;
   }

   V3DVK_FROM_HANDLE(v3dvk_image, image, images[index]);
   free(images);

   return image;
}

static VkResult
v3dvk_image_from_swapchain(VkDevice device,
                           const VkImageCreateInfo *pCreateInfo,
                           const VkImageSwapchainCreateInfoKHR *swapchain_info,
                           const VkAllocationCallbacks *pAllocator,
                           VkImage *pImage)
{
   struct v3dvk_image *swapchain_image = v3dvk_swapchain_get_image(swapchain_info->swapchain, 0);
   assert(swapchain_image);

   assert(swapchain_image->type == pCreateInfo->imageType);
   assert(swapchain_image->vk_format == pCreateInfo->format);
   assert(swapchain_image->extent.width == pCreateInfo->extent.width);
   assert(swapchain_image->extent.height == pCreateInfo->extent.height);
   assert(swapchain_image->extent.depth == pCreateInfo->extent.depth);
   assert(swapchain_image->array_size == pCreateInfo->arrayLayers);
   /* Color attachment is added by the wsi code. */
   assert(swapchain_image->usage == (pCreateInfo->usage | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT));

   VkImageCreateInfo local_create_info;
   local_create_info = *pCreateInfo;
   local_create_info.pNext = NULL;
   /* The following parameters are implictly selected by the wsi code. */
   local_create_info.tiling = VK_IMAGE_TILING_OPTIMAL;
   local_create_info.samples = VK_SAMPLE_COUNT_1_BIT;
   local_create_info.usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

   /* If the image has a particular modifier, specify that modifier. */
   struct wsi_image_create_info local_wsi_info = {
      .sType = VK_STRUCTURE_TYPE_WSI_IMAGE_CREATE_INFO_MESA,
      .modifier_count = 1,
      .modifiers = &swapchain_image->drm_format_mod,
   };
   if (swapchain_image->drm_format_mod != DRM_FORMAT_MOD_INVALID)
      __vk_append_struct(&local_create_info, &local_wsi_info);

   return v3dvk_image_create(device,
      &(struct v3dvk_image_create_info) {
         .vk_info = &local_create_info,
         .external_format = swapchain_image->external_format,
      },
      pAllocator,
      pImage);
}


VkResult
v3dvk_CreateImage(VkDevice device,
                  const VkImageCreateInfo *pCreateInfo,
                  const VkAllocationCallbacks *pAllocator,
                  VkImage *pImage)
{
   const struct VkExternalMemoryImageCreateInfo *create_info =
      vk_find_struct_const(pCreateInfo->pNext, EXTERNAL_MEMORY_IMAGE_CREATE_INFO);

   const VkImageSwapchainCreateInfoKHR *swapchain_info =
      vk_find_struct_const(pCreateInfo->pNext, IMAGE_SWAPCHAIN_CREATE_INFO_KHR);
   if (swapchain_info && swapchain_info->swapchain != VK_NULL_HANDLE)
      return v3dvk_image_from_swapchain(device, pCreateInfo, swapchain_info,
                                        pAllocator, pImage);

   return v3dvk_image_create(device,
      &(struct v3dvk_image_create_info) {
         .vk_info = pCreateInfo,
      },
      pAllocator,
      pImage);
}

void
v3dvk_DestroyImage(VkDevice _device, VkImage _image,
                   const VkAllocationCallbacks *pAllocator)
{
   V3DVK_FROM_HANDLE(v3dvk_device, device, _device);
   V3DVK_FROM_HANDLE(v3dvk_image, image, _image);

   if (!image)
      return;

   for (uint32_t p = 0; p < image->n_planes; ++p) {
#if 0
      if (image->planes[p].bo_is_owned) {
         assert(image->planes[p].address.bo != NULL);
         v3dvk_bo_cache_release(device, &device->bo_cache,
                                image->planes[p].address.bo);
      }
#endif
   }

   vk_free2(&device->alloc, pAllocator, image);
}
