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
#include "v3dvk_math.h"
#include "v3dvk_memory.h"
#include "vk_format_info.h"
#include "vk_format.h"

static inline uint32_t
v3dvk_calc_layer_count(uint32_t image_layer_count,
                       const VkImageSubresourceRange *range)
{
   return range->layerCount == VK_REMAINING_ARRAY_LAYERS
             ? image_layer_count - range->baseArrayLayer
             : range->layerCount;
}

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
   image->samples = pCreateInfo->samples;
   image->usage = pCreateInfo->usage;
   image->create_flags = pCreateInfo->flags;
   image->tiling = pCreateInfo->tiling;
   image->disjoint = pCreateInfo->flags & VK_IMAGE_CREATE_DISJOINT_BIT;
   image->layer_count = pCreateInfo->arrayLayers;
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
   assert(swapchain_image->layer_count == pCreateInfo->arrayLayers);
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

static void
v3dvk_image_view_init(struct v3dvk_image_view *iview,
                      struct v3dvk_device *device,
                      const VkImageViewCreateInfo *pCreateInfo)
{
   V3DVK_FROM_HANDLE(v3dvk_image, image, pCreateInfo->image);
   const VkImageSubresourceRange *range = &pCreateInfo->subresourceRange;

   switch (image->type) {
   case VK_IMAGE_TYPE_1D:
   case VK_IMAGE_TYPE_2D:
      assert(range->baseArrayLayer + v3dvk_calc_layer_count(image->layer_count, range) <=
             image->layer_count);
      break;
   case VK_IMAGE_TYPE_3D:
      assert(range->baseArrayLayer + v3dvk_calc_layer_count(image->layer_count, range) <=
             v3dvk_minify(image->extent.depth, range->baseMipLevel));
      break;
   default:
      unreachable("bad VkImageType");
   }

   iview->image = image;
   iview->type = pCreateInfo->viewType;

   iview->aspect_mask = pCreateInfo->subresourceRange.aspectMask;

   if (iview->aspect_mask == VK_IMAGE_ASPECT_STENCIL_BIT) {
      iview->vk_format = vk_format_stencil_only(iview->vk_format);
   } else if (iview->aspect_mask == VK_IMAGE_ASPECT_DEPTH_BIT) {
      iview->vk_format = vk_format_depth_only(iview->vk_format);
   } else {
      iview->vk_format = pCreateInfo->format;
   }
#if 0
   // should we minify?
   iview->extent = image->extent;

   iview->base_layer = range->baseArrayLayer;
   iview->layer_count = tu_get_layerCount(image, range);
   iview->base_mip = range->baseMipLevel;
   iview->level_count = tu_get_levelCount(image, range);

   memset(iview->descriptor, 0, sizeof(iview->descriptor));

   const struct tu_native_format *fmt = tu6_get_native_format(iview->vk_format);
   assert(fmt && fmt->tex >= 0);

   struct tu_image_level *slice0 = &image->levels[iview->base_mip];
   uint32_t cpp = vk_format_get_blocksize(iview->vk_format);
   uint32_t block_width = vk_format_get_blockwidth(iview->vk_format);
   const VkComponentMapping *comps = &pCreateInfo->components;

   iview->descriptor[0] =
      A6XX_TEX_CONST_0_TILE_MODE(image->tile_mode) |
      COND(vk_format_is_srgb(iview->vk_format), A6XX_TEX_CONST_0_SRGB) |
      A6XX_TEX_CONST_0_FMT(fmt->tex) |
      A6XX_TEX_CONST_0_SAMPLES(0) |
      A6XX_TEX_CONST_0_SWAP(fmt->swap) |
      A6XX_TEX_CONST_0_SWIZ_X(translate_swiz(comps->r, A6XX_TEX_X)) |
      A6XX_TEX_CONST_0_SWIZ_Y(translate_swiz(comps->g, A6XX_TEX_Y)) |
      A6XX_TEX_CONST_0_SWIZ_Z(translate_swiz(comps->b, A6XX_TEX_Z)) |
      A6XX_TEX_CONST_0_SWIZ_W(translate_swiz(comps->a, A6XX_TEX_W)) |
      A6XX_TEX_CONST_0_MIPLVLS(iview->level_count - 1);
   iview->descriptor[1] =
      A6XX_TEX_CONST_1_WIDTH(u_minify(image->extent.width, iview->base_mip)) |
      A6XX_TEX_CONST_1_HEIGHT(u_minify(image->extent.height, iview->base_mip));
   iview->descriptor[2] =
      A6XX_TEX_CONST_2_FETCHSIZE(translate_fetchsize(cpp)) |
      A6XX_TEX_CONST_2_PITCH(slice0->pitch / block_width * cpp) |
      A6XX_TEX_CONST_2_TYPE(translate_tex_type(pCreateInfo->viewType));
   if (pCreateInfo->viewType != VK_IMAGE_VIEW_TYPE_3D) {
      iview->descriptor[3] = A6XX_TEX_CONST_3_ARRAY_PITCH(image->layer_size);
   } else {
      iview->descriptor[3] =
         A6XX_TEX_CONST_3_MIN_LAYERSZ(image->levels[image->level_count - 1].size) |
         A6XX_TEX_CONST_3_ARRAY_PITCH(slice0->size);
   }
   uint64_t base_addr = image->bo->iova + iview->base_layer * image->layer_size + slice0->offset;
   iview->descriptor[4] = base_addr;
   iview->descriptor[5] = base_addr >> 32 | A6XX_TEX_CONST_5_DEPTH(iview->layer_count);
#endif
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

VkResult
v3dvk_CreateImageView(VkDevice _device,
                      const VkImageViewCreateInfo *pCreateInfo,
                      const VkAllocationCallbacks *pAllocator,
                      VkImageView *pView)
{
   V3DVK_FROM_HANDLE(v3dvk_device, device, _device);
   struct v3dvk_image_view *view;

   view = vk_alloc2(&device->alloc, pAllocator, sizeof(*view), 8,
                    VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
   if (view == NULL)
      return v3dvk_error(device->instance, VK_ERROR_OUT_OF_HOST_MEMORY);

   v3dvk_image_view_init(view, device, pCreateInfo);

   *pView = v3dvk_image_view_to_handle(view);

   return VK_SUCCESS;
}

void
v3dvk_DestroyImageView(VkDevice _device,
                       VkImageView _iview,
                       const VkAllocationCallbacks *pAllocator)
{
   V3DVK_FROM_HANDLE(v3dvk_device, device, _device);
   V3DVK_FROM_HANDLE(v3dvk_image_view, iview, _iview);

   if (!iview)
      return;
   vk_free2(&device->alloc, pAllocator, iview);
}

void
v3dvk_GetImageMemoryRequirements(VkDevice _device,
                              VkImage _image,
                              VkMemoryRequirements *pMemoryRequirements)
{
   V3DVK_FROM_HANDLE(v3dvk_image, image, _image);

   pMemoryRequirements->memoryTypeBits = 1;
   pMemoryRequirements->size = image->size;
   pMemoryRequirements->alignment = image->alignment;
}

void
v3dvk_GetImageMemoryRequirements2(VkDevice device,
                               const VkImageMemoryRequirementsInfo2 *pInfo,
                               VkMemoryRequirements2 *pMemoryRequirements)
{
   v3dvk_GetImageMemoryRequirements(device, pInfo->image,
                                    &pMemoryRequirements->memoryRequirements);
}

VkResult
v3dvk_BindImageMemory2(VkDevice device,
                       uint32_t bindInfoCount,
                       const VkBindImageMemoryInfo *pBindInfos)
{
   for (uint32_t i = 0; i < bindInfoCount; ++i) {
      V3DVK_FROM_HANDLE(v3dvk_image, image, pBindInfos[i].image);
      V3DVK_FROM_HANDLE(v3dvk_device_memory, mem, pBindInfos[i].memory);

      if (mem) {
         image->bo = &mem->bo;
         image->bo_offset = pBindInfos[i].memoryOffset;
      } else {
         image->bo = NULL;
         image->bo_offset = 0;
      }
   }

   return VK_SUCCESS;
}

VkResult
v3dvk_BindImageMemory(VkDevice device,
                      VkImage image,
                      VkDeviceMemory memory,
                      VkDeviceSize memoryOffset)
{
   const VkBindImageMemoryInfo info = {
      .sType = VK_STRUCTURE_TYPE_BIND_BUFFER_MEMORY_INFO,
      .image = image,
      .memory = memory,
      .memoryOffset = memoryOffset
   };

   return v3dvk_BindImageMemory2(device, 1, &info);
}
