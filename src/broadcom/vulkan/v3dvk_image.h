#ifndef V3DVK_IMAGE_H
#define V3DVK_IMAGE_H

#include <stdbool.h>
#include <vulkan/vulkan.h>

struct v3dvk_bo;

struct v3dvk_image {
   VkImageType type; /**< VkImageCreateInfo::imageType */
   /* The original VkFormat provided by the client.  This may not match any
    * of the actual surface formats.
    */
   VkFormat vk_format;
   const struct v3dvk_format *format;

   VkImageAspectFlags aspects;
   VkExtent3D extent;
   uint32_t layer_count;
   uint32_t levels;
   uint32_t samples; /**< VkImageCreateInfo::samples */
   uint32_t n_planes;
   VkImageUsageFlags usage; /**< VkImageCreateInfo::usage. */
   VkImageUsageFlags stencil_usage;
   VkImageCreateFlags create_flags; /* Flags used when creating image. */
   VkImageTiling tiling; /** VkImageCreateInfo::tiling */
#if 0
   /** True if this is needs to be bound to an appropriately tiled BO.
    *
    * When not using modifiers, consumers such as X11, Wayland, and KMS need
    * the tiling passed via I915_GEM_SET_TILING.  When exporting these buffers
    * we require a dedicated allocation so that we can know to allocate a
    * tiled buffer.
    */
   bool needs_set_tiling;
#endif
   /**
    * Must be DRM_FORMAT_MOD_INVALID unless tiling is
    * VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT.
    */
   uint64_t drm_format_mod;

   VkDeviceSize size;
   uint32_t alignment;

   /* Whether the image is made of several underlying buffer objects rather a
    * single one with different offsets.
    */
   bool disjoint;
#if 0
   /* All the formats that can be used when creating views of this image
    * are CCS_E compatible.
    */
   bool ccs_e_compatible;
#endif
   /* Image was created with external format. */
   bool external_format;
#if 0
   /**
    * Image subsurfaces
    *
    * For each foo, anv_image::planes[x].surface is valid if and only if
    * anv_image::aspects has a x aspect. Refer to anv_image_aspect_to_plane()
    * to figure the number associated with a given aspect.
    *
    * The hardware requires that the depth buffer and stencil buffer be
    * separate surfaces.  From Vulkan's perspective, though, depth and stencil
    * reside in the same VkImage.  To satisfy both the hardware and Vulkan, we
    * allocate the depth and stencil buffers as separate surfaces in the same
    * bo.
    *
    * Memory layout :
    *
    * -----------------------
    * |     surface0        |   /|\
    * -----------------------    |
    * |   shadow surface0   |    |
    * -----------------------    | Plane 0
    * |    aux surface0     |    |
    * -----------------------    |
    * | fast clear colors0  |   \|/
    * -----------------------
    * |     surface1        |   /|\
    * -----------------------    |
    * |   shadow surface1   |    |
    * -----------------------    | Plane 1
    * |    aux surface1     |    |
    * -----------------------    |
    * | fast clear colors1  |   \|/
    * -----------------------
    * |        ...          |
    * |                     |
    * -----------------------
    */
   struct {
      /**
       * Offset of the entire plane (whenever the image is disjoint this is
       * set to 0).
       */
      uint32_t offset;

      VkDeviceSize size;
      uint32_t alignment;

      struct anv_surface surface;

      /**
       * A surface which shadows the main surface and may have different
       * tiling. This is used for sampling using a tiling that isn't supported
       * for other operations.
       */
      struct anv_surface shadow_surface;

      /**
       * For color images, this is the aux usage for this image when not used
       * as a color attachment.
       *
       * For depth/stencil images, this is set to ISL_AUX_USAGE_HIZ if the
       * image has a HiZ buffer.
       */
      enum isl_aux_usage aux_usage;

      struct anv_surface aux_surface;

      /**
       * Offset of the fast clear state (used to compute the
       * fast_clear_state_offset of the following planes).
       */
      uint32_t fast_clear_state_offset;

      /**
       * BO associated with this plane, set when bound.
       */
      struct anv_address address;

      /**
       * When destroying the image, also free the bo.
       * */
      bool bo_is_owned;
   } planes[3];
#endif
   /* Set when bound */
   struct v3dvk_bo *bo;
   VkDeviceSize bo_offset;
};

struct v3dvk_image_view {
   const struct v3dvk_image *image; /**< VkImageViewCreateInfo::image */

   VkImageViewType type;
   VkImageAspectFlags aspect_mask;
   VkFormat vk_format;
#if 0
   VkExtent3D extent; /**< Extent of VkImageViewCreateInfo::baseMipLevel. */

   unsigned n_planes;
   struct {
      uint32_t image_plane;

      struct isl_view isl;

      /**
       * RENDER_SURFACE_STATE when using image as a sampler surface with an
       * image layout of SHADER_READ_ONLY_OPTIMAL or
       * DEPTH_STENCIL_READ_ONLY_OPTIMAL.
       */
      struct anv_surface_state optimal_sampler_surface_state;

      /**
       * RENDER_SURFACE_STATE when using image as a sampler surface with an
       * image layout of GENERAL.
       */
      struct anv_surface_state general_sampler_surface_state;

      /**
       * RENDER_SURFACE_STATE when using image as a storage image. Separate
       * states for write-only and readable, using the real format for
       * write-only and the lowered format for readable.
       */
      struct anv_surface_state storage_surface_state;
      struct anv_surface_state writeonly_storage_surface_state;

      struct brw_image_param storage_image_param;
   } planes[3];
#endif
};

struct v3dvk_image_create_info {
   const VkImageCreateInfo *vk_info;
#if 0
   /** An opt-in bitmask which filters an ISL-mapping of the Vulkan tiling. */
   isl_tiling_flags_t isl_tiling_flags;

   /** These flags will be added to any derived from VkImageCreateInfo. */
   isl_surf_usage_flags_t isl_extra_usage_flags;
#endif
   uint32_t stride;
   bool external_format;
};

VkResult v3dvk_image_create(VkDevice _device,
                            const struct v3dvk_image_create_info *info,
                            const VkAllocationCallbacks* alloc,
                            VkImage *pImage);

#endif
