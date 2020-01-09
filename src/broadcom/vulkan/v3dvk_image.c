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

#include "util/format/u_format.h"
#include "vulkan/util/vk_alloc.h"
#include "vulkan/util/vk_util.h"
#include "vulkan/wsi/wsi_common.h"
#include "common.h"
#include "device.h"
#include "v3dvk_defines.h"
#include "v3dvk_error.h"
#include "v3dvk_format_table.h"
#include "v3dvk_formats.h"
#include "v3dvk_image.h"
#include "v3dvk_math.h"
#include "v3dvk_memory.h"
#include "vk_format_info.h"
#include "v3d_cl.inl"
#include "v3d_tiling.h"
#include "vk_format.h"

static uint32_t
translate_swizzle(VkComponentSwizzle swizzle)
{
   switch (swizzle) {
   case VK_COMPONENT_SWIZZLE_ZERO:
      return SWIZZLE_ZERO;
   case VK_COMPONENT_SWIZZLE_ONE:
      return SWIZZLE_ONE;
   case VK_COMPONENT_SWIZZLE_R:
      return SWIZZLE_RED;
   case VK_COMPONENT_SWIZZLE_G:
      return SWIZZLE_GREEN;
   case VK_COMPONENT_SWIZZLE_B:
      return SWIZZLE_BLUE;
   case VK_COMPONENT_SWIZZLE_A:
      return SWIZZLE_ALPHA;
   default:
      unreachable("unknown swizzle");
   }
}


/* These are tunable parameters in the HW design, but all the V3D
 * implementations agree.
 */
#define VC5_UIFCFG_BANKS 8
#define VC5_UIFCFG_PAGE_SIZE 4096
#define VC5_UIFCFG_XOR_VALUE (1 << 4)
#define VC5_PAGE_CACHE_SIZE (VC5_UIFCFG_PAGE_SIZE * VC5_UIFCFG_BANKS)
#define VC5_UBLOCK_SIZE 64
#define VC5_UIFBLOCK_SIZE (4 * VC5_UBLOCK_SIZE)
#define VC5_UIFBLOCK_ROW_SIZE (4 * VC5_UIFBLOCK_SIZE)

#define PAGE_UB_ROWS (VC5_UIFCFG_PAGE_SIZE / VC5_UIFBLOCK_ROW_SIZE)
#define PAGE_UB_ROWS_TIMES_1_5 ((PAGE_UB_ROWS * 3) >> 1)
#define PAGE_CACHE_UB_ROWS (VC5_PAGE_CACHE_SIZE / VC5_UIFBLOCK_ROW_SIZE)
#define PAGE_CACHE_MINUS_1_5_UB_ROWS (PAGE_CACHE_UB_ROWS - PAGE_UB_ROWS_TIMES_1_5)

/**
 * Computes the HW's UIFblock padding for a given height/cpp.
 *
 * The goal of the padding is to keep pages of the same color (bank number) at
 * least half a page away from each other vertically when crossing between
 * columns of UIF blocks.
 */
static uint32_t
v3d_get_ub_pad(struct v3dvk_image *image, uint32_t height)
{
        uint32_t utile_h = v3d_utile_height(image->cpp);
        uint32_t uif_block_h = utile_h * 2;
        uint32_t height_ub = height / uif_block_h;

        uint32_t height_offset_in_pc = height_ub % PAGE_CACHE_UB_ROWS;

        /* For the perfectly-aligned-for-UIF-XOR case, don't add any pad. */
        if (height_offset_in_pc == 0)
                return 0;

        /* Try padding up to where we're offset by at least half a page. */
        if (height_offset_in_pc < PAGE_UB_ROWS_TIMES_1_5) {
                /* If we fit entirely in the page cache, don't pad. */
                if (height_ub < PAGE_CACHE_UB_ROWS)
                        return 0;
                else
                        return PAGE_UB_ROWS_TIMES_1_5 - height_offset_in_pc;
        }

        /* If we're close to being aligned to page cache size, then round up
         * and rely on XOR.
         */
        if (height_offset_in_pc > PAGE_CACHE_MINUS_1_5_UB_ROWS)
                return PAGE_CACHE_UB_ROWS - height_offset_in_pc;

        /* Otherwise, we're far enough away (top and bottom) to not need any
         * padding.
         */
        return 0;
}

static void
v3d_setup_slices(struct v3dvk_image * image, uint32_t winsys_stride,
                 bool uif_top)
{
        uint32_t width = image->extent.width;
        uint32_t height = image->extent.height;
        uint32_t depth = image->extent.depth;
        /* Note that power-of-two padding is based on level 1.  These are not
         * equivalent to just util_next_power_of_two(dimension), because at a
         * level 0 dimension of 9, the level 1 power-of-two padded value is 4,
         * not 8.
         */
        uint32_t pot_width = 2 * util_next_power_of_two(u_minify(width, 1));
        uint32_t pot_height = 2 * util_next_power_of_two(u_minify(height, 1));
        uint32_t pot_depth = 2 * util_next_power_of_two(u_minify(depth, 1));
        uint32_t offset = 0;
        uint32_t utile_w = v3d_utile_width(image->cpp);
        uint32_t utile_h = v3d_utile_height(image->cpp);
        uint32_t uif_block_w = utile_w * 2;
        uint32_t uif_block_h = utile_h * 2;
        uint32_t block_width = util_format_get_blockwidth(vk_format_to_pipe_format(image->vk_format));
        uint32_t block_height = util_format_get_blockheight(vk_format_to_pipe_format(image->vk_format));
        bool msaa = image->samples > 1;

        /* MSAA textures/renderbuffers are always laid out as single-level
         * UIF.
         */
        uif_top |= msaa;

        /* Check some easy mistakes to make in a resource_create() call that
         * will break our setup.
         */
        assert(image->array_size != 0);
        assert(image->extent.depth != 0);

        for (int i = image->level_count - 1; i >= 0; i--) {
                struct v3d_resource_slice *slice = &image->slices[i];

                uint32_t level_width, level_height, level_depth;
                if (i < 2) {
                        level_width = u_minify(width, i);
                        level_height = u_minify(height, i);
                } else {
                        level_width = u_minify(pot_width, i);
                        level_height = u_minify(pot_height, i);
                }
                if (i < 1)
                        level_depth = u_minify(depth, i);
                else
                        level_depth = u_minify(pot_depth, i);

                if (msaa) {
                        level_width *= 2;
                        level_height *= 2;
                }

                level_width = DIV_ROUND_UP(level_width, block_width);
                level_height = DIV_ROUND_UP(level_height, block_height);

                if (!image->tiling == VK_IMAGE_TILING_OPTIMAL) {
                        slice->tiling = VC5_TILING_RASTER;
                        if (image->type == VK_IMAGE_TYPE_1D)
                                level_width = align(level_width, 64 / image->cpp);
                } else {
                        if ((i != 0 || !uif_top) &&
                            (level_width <= utile_w ||
                             level_height <= utile_h)) {
                                slice->tiling = VC5_TILING_LINEARTILE;
                                level_width = align(level_width, utile_w);
                                level_height = align(level_height, utile_h);
                        } else if ((i != 0 || !uif_top) &&
                                   level_width <= uif_block_w) {
                                slice->tiling = VC5_TILING_UBLINEAR_1_COLUMN;
                                level_width = align(level_width, uif_block_w);
                                level_height = align(level_height, uif_block_h);
                        } else if ((i != 0 || !uif_top) &&
                                   level_width <= 2 * uif_block_w) {
                                slice->tiling = VC5_TILING_UBLINEAR_2_COLUMN;
                                level_width = align(level_width, 2 * uif_block_w);
                                level_height = align(level_height, uif_block_h);
                        } else {
                                /* We align the width to a 4-block column of
                                 * UIF blocks, but we only align height to UIF
                                 * blocks.
                                 */
                                level_width = align(level_width,
                                                    4 * uif_block_w);
                                level_height = align(level_height,
                                                     uif_block_h);

                                slice->ub_pad = v3d_get_ub_pad(image,
                                                               level_height);
                                level_height += slice->ub_pad * uif_block_h;

                                /* If the padding set us to to be aligned to
                                 * the page cache size, then the HW will use
                                 * the XOR bit on odd columns to get us
                                 * perfectly misaligned
                                 */
                                if ((level_height / uif_block_h) %
                                    (VC5_PAGE_CACHE_SIZE /
                                     VC5_UIFBLOCK_ROW_SIZE) == 0) {
                                        slice->tiling = VC5_TILING_UIF_XOR;
                                } else {
                                        slice->tiling = VC5_TILING_UIF_NO_XOR;
                                }
                        }
                }

                slice->offset = offset;
                if (winsys_stride)
                        slice->stride = winsys_stride;
                else
                        slice->stride = level_width * image->cpp;
                slice->padded_height = level_height;
                slice->size = level_height * slice->stride;

                uint32_t slice_total_size = slice->size * level_depth;

                /* The HW aligns level 1's base to a page if any of level 1 or
                 * below could be UIF XOR.  The lower levels then inherit the
                 * alignment for as long as necesary, thanks to being power of
                 * two aligned.
                 */
                if (i == 1 &&
                    level_width > 4 * uif_block_w &&
                    level_height > PAGE_CACHE_MINUS_1_5_UB_ROWS * uif_block_h) {
                        slice_total_size = align(slice_total_size,
                                                 VC5_UIFCFG_PAGE_SIZE);
                }

                offset += slice_total_size;

        }
        image->size = offset;

        /* UIF/UBLINEAR levels need to be aligned to UIF-blocks, and LT only
         * needs to be aligned to utile boundaries.  Since tiles are laid out
         * from small to big in memory, we need to align the later UIF slices
         * to UIF blocks, if they were preceded by non-UIF-block-aligned LT
         * slices.
         *
         * We additionally align to 4k, which improves UIF XOR performance.
         */
        uint32_t page_align_offset = (align(image->slices[0].offset, 4096) -
                                      image->slices[0].offset);
        if (page_align_offset) {
                image->size += page_align_offset;
                for (int i = 0; i <= image->level_count - 1; i++)
                        image->slices[i].offset += page_align_offset;
        }

        /* Arrays and cube textures have a stride which is the distance from
         * one full mipmap tree to the next (64b aligned).  For 3D textures,
         * we need to program the stride between slices of miplevel 0.
         */
        if (image->type != VK_IMAGE_TYPE_3D) {
                image->cube_map_stride = align(image->slices[0].offset +
                                             image->slices[0].size, 64);
                image->size += image->cube_map_stride * (image->array_size - 1);
        } else {
                image->cube_map_stride = image->slices[0].size;
        }
}

static uint32_t
v3d_layer_offset(const struct v3dvk_image* image, VkImageViewType viewType, uint32_t level, uint32_t layer)
{
   const struct v3dvk_image_level* slice = &image->levels[level];

   if (viewType == VK_IMAGE_VIEW_TYPE_3D)
      return slice->offset + layer * slice->size;
   else
      return slice->offset + layer * 1; //FIXME: what is rsc->cube_map_stride?
}

static void
v3dvk_setup_texture_shader_state(struct V3D42_TEXTURE_SHADER_STATE *tex,
                                 const struct v3dvk_image* image,
                                 VkImageViewType viewType,
                                 int base_level, int last_level,
                                 int first_layer, int last_layer)
{
        int msaa_scale = image->samples > 1 ? 2 : 1;

        tex->image_width = image->extent.width * msaa_scale;
        tex->image_height = image->extent.height * msaa_scale;

        /* On 4.x, the height of a 1D texture is redefined to be the
         * upper 14 bits of the width (which is only usable with txf).
         */
        if (viewType == VK_IMAGE_VIEW_TYPE_1D ||
            viewType == VK_IMAGE_VIEW_TYPE_1D_ARRAY) {
                tex->image_height = tex->image_width >> 14;
        }

        tex->image_width &= (1 << 14) - 1;
        tex->image_height &= (1 << 14) - 1;

        if (viewType == VK_IMAGE_VIEW_TYPE_3D) {
                tex->image_depth = image->extent.depth;
        } else {
                tex->image_depth = (last_layer - first_layer) + 1;
        }

        tex->base_level = base_level;
        tex->max_level = last_level;
        /* Note that we don't have a job to reference the texture's sBO
         * at state create time, so any time this sampler view is used
         * we need to add the texture to the job.
         */
        tex->texture_base_pointer =
                cl_address(NULL,
                           image->bo->offset +
                           v3d_layer_offset(image, viewType, 0, first_layer));
        tex->array_stride_64_byte_aligned = image->cube_map_stride / 64;

        /* Since other platform devices may produce UIF images even
         * when they're not big enough for V3D to assume they're UIF,
         * we force images with level 0 as UIF to be always treated
         * that way.
         */
        tex->level_0_is_strictly_uif =
                (image->slices[0].tiling == VC5_TILING_UIF_XOR ||
                 image->slices[0].tiling == VC5_TILING_UIF_NO_XOR);
        tex->level_0_xor_enable = (image->slices[0].tiling == VC5_TILING_UIF_XOR);

        if (tex->level_0_is_strictly_uif)
                tex->level_0_ub_pad = image->slices[0].ub_pad;

        if (tex->uif_xor_disable ||
            tex->level_0_is_strictly_uif) {
                tex->extended = true;
        }
}

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

    // TODO: Check whether this value is actually correct
   image->array_size = 1;
   image->type = pCreateInfo->imageType;
   image->extent = pCreateInfo->extent;
   image->vk_format = pCreateInfo->format;
   image->format = v3dvk_get_format(pCreateInfo->format);
   image->aspects = vk_format_aspects(image->vk_format);
   image->level_count = pCreateInfo->mipLevels;
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

    image->cpp = util_format_get_blocksize(vk_format_to_pipe_format(image->vk_format));

   assert(image->cpp);

   // FIXME: Check whether all params are necessary
   v3d_setup_slices(image, 0, false);
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
   // should we minify?
   iview->extent = image->extent;

   iview->base_layer = range->baseArrayLayer;
#if 0
   iview->layer_count = tu_get_layerCount(image, range);
#endif
   iview->base_mip = range->baseMipLevel;
#if 0
   iview->level_count = tu_get_levelCount(image, range);
#endif
   memset(iview->descriptor, 0, sizeof(iview->descriptor));
#if 0
   const struct tu_native_format *fmt = tu6_get_native_format(iview->vk_format);
   assert(fmt && fmt->tex >= 0);

   struct tu_image_level *slice0 = &image->levels[iview->base_mip];
   uint32_t cpp = vk_format_get_blocksize(iview->vk_format);
#endif
   uint32_t block_width = vk_format_get_blockwidth(iview->vk_format);
   const VkComponentMapping *comps = &pCreateInfo->components;

   struct vk_format_description dummy;

   const struct vk_format_description * d = v3d41_get_format_desc(iview->vk_format);
   if (unlikely(!d && device->robust_buffer_access)) {
      fprintf(stderr, "No format description found for %i", iview->vk_format);
      memset(&dummy, 0, sizeof(dummy));
      d = &dummy;
   }

   v3dx_pack(iview->descriptor, TEXTURE_SHADER_STATE, tex) {
      v3dvk_setup_texture_shader_state(&tex, iview->image,
                                       iview->type,
                                       iview->base_mip,
                                       iview->base_mip + iview->level_count,
                                       iview->base_layer,
                                       iview->base_layer + iview->level_count);

      tex.swizzle_r = translate_swizzle(VK_COMPONENT_SWIZZLE_R);
      tex.swizzle_g = translate_swizzle(VK_COMPONENT_SWIZZLE_G);
      tex.swizzle_b = translate_swizzle(VK_COMPONENT_SWIZZLE_B);
      tex.swizzle_a = translate_swizzle(VK_COMPONENT_SWIZZLE_A);

      tex.texture_type = d->tex_type;
#if 0
      A6XX_TEX_CONST_0_TILE_MODE(image->tile_mode) |
#endif
      tex.srgb = vk_format_is_srgb(iview->vk_format);
#if 0
      A6XX_TEX_CONST_0_SAMPLES(0) |
      A6XX_TEX_CONST_0_SWAP(fmt->swap) |
      A6XX_TEX_CONST_0_MIPLVLS(iview->level_count - 1);
#endif
   };
#if 0
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
