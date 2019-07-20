
#include <assert.h>
#include "common.h"
#include "device.h"
#include "vk_alloc.h"
#include "v3dvk_error.h"
#include "v3dvk_sampler.h"

static enum V3D42_Compare_Function
translate_compare(enum VkCompareOp op) {
   switch (op) {
   case VK_COMPARE_OP_NEVER:
      return V3D_COMPARE_FUNC_NEVER;
   case VK_COMPARE_OP_LESS:
      return V3D_COMPARE_FUNC_LESS;
   case VK_COMPARE_OP_EQUAL:
      return V3D_COMPARE_FUNC_EQUAL;
   case VK_COMPARE_OP_LESS_OR_EQUAL:
      return V3D_COMPARE_FUNC_LEQUAL;
   case VK_COMPARE_OP_GREATER:
      return V3D_COMPARE_FUNC_GREATER;
   case VK_COMPARE_OP_NOT_EQUAL:
      return V3D_COMPARE_FUNC_NOTEQUAL;
   case VK_COMPARE_OP_GREATER_OR_EQUAL:
      return V3D_COMPARE_FUNC_GEQUAL;
   case VK_COMPARE_OP_ALWAYS:
      return V3D_COMPARE_FUNC_ALWAYS;
   default:
      unreachable("Unknown compare op");
  }
}

static enum V3D42_Wrap_Mode
translate_wrap(enum VkSamplerAddressMode mode, bool using_nearest)
{
   switch (mode) {
   case VK_SAMPLER_ADDRESS_MODE_REPEAT:
      return V3D_WRAP_MODE_REPEAT;
   case VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE:
      return V3D_WRAP_MODE_CLAMP;
   case VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT:
      return V3D_WRAP_MODE_MIRROR;
   case VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER:
      return V3D_WRAP_MODE_BORDER;
#if 0
        case PIPE_TEX_WRAP_CLAMP:
                return (using_nearest ?
                        V3D_WRAP_MODE_CLAMP :
                        V3D_WRAP_MODE_BORDER);
#endif
   default:
      unreachable("Unknown wrap mode");
   }
}

static enum V3D42_Border_Color_Mode
translate_border(enum VkBorderColor color) {
   switch(color) {
   case VK_BORDER_COLOR_INT_TRANSPARENT_BLACK:
      return V3D_BORDER_COLOR_0000;
   case VK_BORDER_COLOR_INT_OPAQUE_BLACK:
      return V3D_BORDER_COLOR_0001;
   case VK_BORDER_COLOR_INT_OPAQUE_WHITE:
      return V3D_BORDER_COLOR_1111;
   default:
      unreachable("Unknown color");
   }
}

static void
v3dvk_init_sampler(struct v3dvk_device *device,
                   struct v3dvk_sampler *sampler,
                   const VkSamplerCreateInfo *pCreateInfo)
{

   bool either_nearest = false;
#if 0
                (cso->mag_img_filter == PIPE_TEX_MIPFILTER_NEAREST ||
                 cso->min_img_filter == PIPE_TEX_MIPFILTER_NEAREST);

#endif
   sampler->state.wrap_i_border = false;
   
   sampler->state.wrap_s = translate_wrap(pCreateInfo->addressModeV, either_nearest);
   sampler->state.wrap_t = translate_wrap(pCreateInfo->addressModeW, either_nearest);
   sampler->state.wrap_r = translate_wrap(pCreateInfo->addressModeU, either_nearest);

   sampler->state.fixed_bias = pCreateInfo->mipLodBias;

   sampler->state.depth_compare_function = translate_compare(pCreateInfo->compareOp);

   sampler->state.min_filter_nearest =
      pCreateInfo->minFilter == VK_FILTER_NEAREST;
   sampler->state.mag_filter_nearest = 
      pCreateInfo->magFilter == VK_FILTER_NEAREST;

   sampler->state.mip_filter_nearest =
      pCreateInfo->mipmapMode != VK_SAMPLER_MIPMAP_MODE_LINEAR;

   sampler->state.min_level_of_detail = MIN2(MAX2(0, pCreateInfo->minLod),
                                                   15);
   sampler->state.max_level_of_detail = MIN2(pCreateInfo->maxLod, 15);
#if 0
                /* If we're not doing inter-miplevel filtering, we need to
                 * clamp the LOD so that we only sample from baselevel.
                 * However, we need to still allow the calculated LOD to be
                 * fractionally over the baselevel, so that the HW can decide
                 * between the min and mag filters.
                 */
                if (cso->min_mip_filter == PIPE_TEX_MIPFILTER_NONE) {
                        sampler.min_level_of_detail =
                                MIN2(sampler.min_level_of_detail, 1.0 / 256.0);
                        sampler.max_level_of_detail =
                                MIN2(sampler.max_level_of_detail, 1.0 / 256.0);
                }
#endif
   sampler->state.anisotropy_enable = pCreateInfo->anisotropyEnable;

   if (sampler->state.anisotropy_enable) {
      if (pCreateInfo->maxAnisotropy > 8)
         sampler->state.maximum_anisotropy = 3;
      else if (pCreateInfo->maxAnisotropy > 4)
         sampler->state.maximum_anisotropy = 2;
      else if (pCreateInfo->maxAnisotropy > 2)
         sampler->state.maximum_anisotropy = 1;
   }

   // Negated only to keep code similar to Gallium
   const bool noBorder = pCreateInfo->addressModeU != VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER &&
      pCreateInfo->addressModeV != VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER &&
      pCreateInfo->addressModeW != VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;

   if (noBorder) {
      sampler->state.border_color_mode = V3D_BORDER_COLOR_0000;
   } else {
       sampler->state.border_color_mode = translate_border(pCreateInfo->borderColor);
#if 0
      sampler->state.border_color_mode = V3D_BORDER_COLOR_FOLLOWS;

                        union pipe_color_union border;

                        /* First, reswizzle the border color for any
                         * mismatching we're doing between the texture's
                         * channel order in hardware (R) versus what it is at
                         * the GL level (ALPHA)
                         */
                        switch (variant) {
                        case V3D_SAMPLER_STATE_F16_BGRA:
                        case V3D_SAMPLER_STATE_F16_BGRA_UNORM:
                        case V3D_SAMPLER_STATE_F16_BGRA_SNORM:
                                border.i[0] = cso->border_color.i[2];
                                border.i[1] = cso->border_color.i[1];
                                border.i[2] = cso->border_color.i[0];
                                border.i[3] = cso->border_color.i[3];
                                break;

                        case V3D_SAMPLER_STATE_F16_A:
                        case V3D_SAMPLER_STATE_F16_A_UNORM:
                        case V3D_SAMPLER_STATE_F16_A_SNORM:
                        case V3D_SAMPLER_STATE_32_A:
                        case V3D_SAMPLER_STATE_32_A_UNORM:
                        case V3D_SAMPLER_STATE_32_A_SNORM:
                                border.i[0] = cso->border_color.i[3];
                                border.i[1] = 0;
                                border.i[2] = 0;
                                border.i[3] = 0;
                                break;

                        case V3D_SAMPLER_STATE_F16_LA:
                        case V3D_SAMPLER_STATE_F16_LA_UNORM:
                        case V3D_SAMPLER_STATE_F16_LA_SNORM:
                                border.i[0] = cso->border_color.i[0];
                                border.i[1] = cso->border_color.i[3];
                                border.i[2] = 0;
                                border.i[3] = 0;
                                break;

                        default:
                                border = cso->border_color;
                        }

                        /* Perform any clamping. */
                        switch (variant) {
                        case V3D_SAMPLER_STATE_F16_UNORM:
                        case V3D_SAMPLER_STATE_F16_BGRA_UNORM:
                        case V3D_SAMPLER_STATE_F16_A_UNORM:
                        case V3D_SAMPLER_STATE_F16_LA_UNORM:
                        case V3D_SAMPLER_STATE_32_UNORM:
                        case V3D_SAMPLER_STATE_32_A_UNORM:
                                for (int i = 0; i < 4; i++)
                                        border.f[i] = CLAMP(border.f[i], 0, 1);
                                break;

                        case V3D_SAMPLER_STATE_F16_SNORM:
                        case V3D_SAMPLER_STATE_F16_BGRA_SNORM:
                        case V3D_SAMPLER_STATE_F16_A_SNORM:
                        case V3D_SAMPLER_STATE_F16_LA_SNORM:
                        case V3D_SAMPLER_STATE_32_SNORM:
                        case V3D_SAMPLER_STATE_32_A_SNORM:
                                for (int i = 0; i < 4; i++)
                                        border.f[i] = CLAMP(border.f[i], -1, 1);
                                break;

                        case V3D_SAMPLER_STATE_1010102U:
                                border.ui[0] = CLAMP(border.ui[0],
                                                     0, (1 << 10) - 1);
                                border.ui[1] = CLAMP(border.ui[1],
                                                     0, (1 << 10) - 1);
                                border.ui[2] = CLAMP(border.ui[2],
                                                     0, (1 << 10) - 1);
                                border.ui[3] = CLAMP(border.ui[3],
                                                     0, 3);
                                break;

                        case V3D_SAMPLER_STATE_16U:
                                for (int i = 0; i < 4; i++)
                                        border.ui[i] = CLAMP(border.ui[i],
                                                             0, 0xffff);
                                break;

                        case V3D_SAMPLER_STATE_16I:
                                for (int i = 0; i < 4; i++)
                                        border.i[i] = CLAMP(border.i[i],
                                                            -32768, 32767);
                                break;

                        case V3D_SAMPLER_STATE_8U:
                                for (int i = 0; i < 4; i++)
                                        border.ui[i] = CLAMP(border.ui[i],
                                                             0, 0xff);
                                break;

                        case V3D_SAMPLER_STATE_8I:
                                for (int i = 0; i < 4; i++)
                                        border.i[i] = CLAMP(border.i[i],
                                                            -128, 127);
                                break;

                        default:
                                break;
                        }

                        if (variant >= V3D_SAMPLER_STATE_32) {
                                sampler.border_color_word_0 = border.ui[0];
                                sampler.border_color_word_1 = border.ui[1];
                                sampler.border_color_word_2 = border.ui[2];
                                sampler.border_color_word_3 = border.ui[3];
                        } else {
                                sampler.border_color_word_0 =
                                        util_float_to_half(border.f[0]);
                                sampler.border_color_word_1 =
                                        util_float_to_half(border.f[1]);
                                sampler.border_color_word_2 =
                                        util_float_to_half(border.f[2]);
                                sampler.border_color_word_3 =
                                        util_float_to_half(border.f[3]);
                        }
                }
#endif
   }
}

VkResult
v3dvk_CreateSampler(VkDevice _device,
                    const VkSamplerCreateInfo *pCreateInfo,
                    const VkAllocationCallbacks *pAllocator,
                    VkSampler *pSampler)
{
   V3DVK_FROM_HANDLE(v3dvk_device, device, _device);
   struct v3dvk_sampler *sampler;

   assert(pCreateInfo->sType == VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO);

   sampler = vk_alloc2(&device->alloc, pAllocator, sizeof(*sampler), 8,
                       VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
   if (!sampler)
      return v3dvk_error(device->instance, VK_ERROR_OUT_OF_HOST_MEMORY);

   v3dvk_init_sampler(device, sampler, pCreateInfo);
   *pSampler = v3dvk_sampler_to_handle(sampler);

   return VK_SUCCESS;
}

void
v3dvk_DestroySampler(VkDevice _device,
                     VkSampler _sampler,
                     const VkAllocationCallbacks *pAllocator)
{
   V3DVK_FROM_HANDLE(v3dvk_device, device, _device);
   V3DVK_FROM_HANDLE(v3dvk_sampler, sampler, _sampler);

   if (!sampler)
      return;
   vk_free2(&device->alloc, pAllocator, sampler);
}
