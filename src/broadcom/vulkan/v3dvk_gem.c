
#include "v3dvk_gem.h"

int
v3dvk_gem_gpu_get_reset_stats(struct v3dvk_device *device,
                              uint32_t *active, uint32_t *pending)
{
#if 0
   struct drm_i915_reset_stats stats = {
      .ctx_id = device->context_id,
   };

   int ret = anv_ioctl(device->fd, DRM_IOCTL_I915_GET_RESET_STATS, &stats);
   if (ret == 0) {
      *active = stats.batch_active;
      *pending = stats.batch_pending;
   }

   return ret;
#endif
    return 1;
}
