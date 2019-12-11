
#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <xf86drm.h>
#include <drm-uapi/v3d_drm.h>
#include <vulkan/vulkan.h>
#include "util/u_math.h"
#include "util/u_memory.h"
#include "common.h"
#include "instance.h"
#include "device.h"
#include "v3dvk_bo.h"
#include "v3dvk_gem.h"

#ifdef HAVE_VALGRIND
#include <valgrind.h>
#include <memcheck.h>
#define VG(x) x
#else
#define VG(x)
#endif

// FIXME: Disable this once we are more stable
static bool dump_stats = true;

static VkResult
v3dvk_bo_init(struct v3dvk_bo *bo,
           const char* name,
           uint64_t size)
{
   *bo = (struct v3dvk_bo) {
      .name = name,
      .size = size,
   };

   return VK_SUCCESS;
}

VkResult
v3dvk_bo_init_new(struct v3dvk_device *dev, struct v3dvk_bo *bo, uint64_t size, const char* name)
{
   assert(bo);
   assert(name);
   /* The CLIF dumping requires that there is no whitespace in the name.
    */
   assert(!strchr(name, ' '));

   size = align(size, 4096);

   VkResult result = v3dvk_bo_init(bo, name, size);
   if (result != VK_SUCCESS) {
      return result;
   }

   bo->private = true;

   struct drm_v3d_create_bo create = {
      .flags = 0,
      .size = size
   };

   int ret = drmIoctl(dev->fd, DRM_IOCTL_V3D_CREATE_BO, &create);
   if (ret != 0)
      fprintf(stderr, "create object %s: %s\n", bo->name, strerror(errno));

   bo->handle = create.handle;
   bo->offset = create.offset;
   bo->dev    = dev;

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

static int v3dvk_wait_bo_ioctl(int fd, uint32_t handle, uint64_t timeout_ns)
{
   struct drm_v3d_wait_bo wait = {
      .handle = handle,
      .timeout_ns = timeout_ns,
   };
   int ret = drmIoctl(fd, DRM_IOCTL_V3D_WAIT_BO, &wait);
   if (ret == -1)
      return -errno;
   else
      return 0;
}

static bool
v3dvk_bo_wait(const struct v3dvk_bo *bo, uint64_t timeout_ns, const char *reason)
{
   if (unlikely(bo->dev->instance->debug_flags & V3DVK_DEBUG_PERF) && timeout_ns && reason) {
      if (v3dvk_wait_bo_ioctl(bo->dev->fd, bo->handle, 0) == -ETIME) {
         fprintf(stderr, "Blocking on %s BO for %s\n",
                 bo->name, reason);
      }
   }

   int ret = v3dvk_wait_bo_ioctl(bo->dev->fd, bo->handle, timeout_ns);
   if (ret) {
      if (ret != -ETIME) {
         fprintf(stderr, "wait failed: %d\n", ret);
         abort();
      }

      return false;
   }

   return true;
}

void
v3dvk_bo_map_unsynchronized(struct v3dvk_bo *bo)
{
   uint64_t offset;
   int ret;

   if (bo->map)
      return bo->map;

   struct drm_v3d_mmap_bo map;
   memset(&map, 0, sizeof(map));
   map.handle = bo->handle;
   ret = drmIoctl(bo->dev->fd, DRM_IOCTL_V3D_MMAP_BO, &map);
   offset = map.offset;
   if (ret != 0) {
      fprintf(stderr, "map ioctl failure\n");
      abort();
   }

   bo->map = mmap(NULL, bo->size, PROT_READ | PROT_WRITE, MAP_SHARED,
                  bo->dev->fd, offset);
   if (bo->map == MAP_FAILED) {
      fprintf(stderr, "mmap of bo %d (offset 0x%016llx, size %d) failed\n",
              bo->handle, (long long)offset, bo->size);
      abort();
   }
   VG(VALGRIND_MALLOCLIKE_BLOCK(bo->map, bo->size, 0, false));

   return bo->map;
}

void
v3dvk_bo_map(struct v3dvk_bo *bo)
{
   v3dvk_bo_map_unsynchronized(bo);

   bool ok = v3dvk_bo_wait(bo, PIPE_TIMEOUT_INFINITE, "bo map");
   if (!ok) {
      fprintf(stderr, "BO wait for map failed\n");
      abort();
   }
}
