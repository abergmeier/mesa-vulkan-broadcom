
#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <xf86drm.h>
#include <drm-uapi/v3d_drm.h>
#include <vulkan/vulkan.h>
#include "util/u_math.h"
#include "util/u_memory.h"
#include "common.h"
#include "v3dvk_bo.h"
#include "v3dvk_gem.h"

static bool dump_stats = false;

static VkResult
v3dvk_bo_init(struct v3dvk_bo *bo,
           uint64_t size)
{
   *bo = (struct v3dvk_bo) {
      .size = size,
   };

   return VK_SUCCESS;
}

VkResult
v3dvk_bo_init_new(struct v3dvk_device *dev, struct v3dvk_bo *bo, uint64_t size)
{
    assert(bo);
   /* The CLIF dumping requires that there is no whitespace in the name.
    */
   assert(!strchr(bo->name, ' '));

   size = align(size, 4096);

   VkResult result = v3dvk_bo_init(bo, size);
   if (result != VK_SUCCESS) {
      return result;
   }

   struct drm_v3d_create_bo create = {
           .size = size
   };

   int ret = drmIoctl(dev->fd, DRM_IOCTL_V3D_CREATE_BO, &create);
   if (ret != 0)
      fprintf(stderr, "create object %s: %s\n", bo->name, strerror(errno));

   bo->handle = create.handle;
   bo->offset = create.offset;

   if (dump_stats) {
           fprintf(stderr, "Allocated %s %llukb:\n", bo->name, size / 1024);
   }

   return VK_SUCCESS;
}

void
v3dvk_bo_finish(struct v3dvk_device *dev, struct v3dvk_bo *bo)
{
        v3dvk_gem_close(dev, bo->handle);

        if (dump_stats) {
                fprintf(stderr, "Freed %s%s%dkb:\n",
                        bo->name ? bo->name : "",
                        bo->name ? " " : "",
                        bo->size / 1024);
        }
}
