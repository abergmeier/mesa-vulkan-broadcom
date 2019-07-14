/*
 * Copyright © 2019 Andreas Bergmeier
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

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include <xf86drm.h>

#include <drm-uapi/v3d_drm.h>

#include "common.h"
#include "device.h"
#include "dev/gen_device_info.h"
#include "ioctl.h"
#include <util/build_id.h>
#include <util/macros.h>
#include <util/mesa-sha1.h>
#include "wsi.h"

static uint32_t
v3d_get_version(int fd)
{
        struct drm_v3d_get_param ident0 = {
                .param = DRM_V3D_PARAM_V3D_CORE0_IDENT0,
        };
        struct drm_v3d_get_param ident1 = {
                .param = DRM_V3D_PARAM_V3D_CORE0_IDENT1,
        };
        int ret;

        ret = v3dvk_ioctl(fd, DRM_IOCTL_V3D_GET_PARAM, &ident0);
        if (ret != 0) {
                fprintf(stderr, "Couldn't get V3D core IDENT0: %s\n",
                        strerror(errno));
                return false;
        }
        ret = v3dvk_ioctl(fd, DRM_IOCTL_V3D_GET_PARAM, &ident1);
        if (ret != 0) {
                fprintf(stderr, "Couldn't get V3D core IDENT1: %s\n",
                        strerror(errno));
                return false;
        }

        uint32_t major = (ident0.value >> 24) & 0xff;
        uint32_t minor = (ident1.value >> 0) & 0xf;
        return major * (uint32_t)10 + minor;
}

static VkResult
v3dvk_physical_device_init_uuids(struct v3dvk_physical_device *device)
{
   const struct build_id_note *note =
      build_id_find_nhdr_for_addr(v3dvk_physical_device_init_uuids);
   if (!note) {
      return vk_errorf(device->instance, device,
                       VK_ERROR_INITIALIZATION_FAILED,
                       "Failed to find build-id");
   }

   unsigned build_id_len = build_id_length(note);
   if (build_id_len < 20) {
      return vk_errorf(device->instance, device,
                       VK_ERROR_INITIALIZATION_FAILED,
                       "build-id too short.  It needs to be a SHA");
   }

   memcpy(device->driver_build_sha1, build_id_data(note), 20);

   struct mesa_sha1 sha1_ctx;
   uint8_t sha1[20];
   STATIC_ASSERT(VK_UUID_SIZE <= sizeof(sha1));

   /* The pipeline cache UUID is used for determining when a pipeline cache is
    * invalid.  It needs both a driver build and the PCI ID of the device.
    */
   _mesa_sha1_init(&sha1_ctx);
   _mesa_sha1_update(&sha1_ctx, build_id_data(note), build_id_len);
   _mesa_sha1_update(&sha1_ctx, &device->chipset_id,
                     sizeof(device->chipset_id));
#if 0
   _mesa_sha1_update(&sha1_ctx, &device->always_use_bindless,
                     sizeof(device->always_use_bindless));
   _mesa_sha1_update(&sha1_ctx, &device->has_a64_buffer_access,
                     sizeof(device->has_a64_buffer_access));
   _mesa_sha1_update(&sha1_ctx, &device->has_bindless_images,
                     sizeof(device->has_bindless_images));
   _mesa_sha1_update(&sha1_ctx, &device->has_bindless_samplers,
                     sizeof(device->has_bindless_samplers));
#endif
   _mesa_sha1_final(&sha1_ctx, sha1);
   memcpy(device->pipeline_cache_uuid, sha1, VK_UUID_SIZE);

   /* The driver UUID is used for determining sharability of images and memory
    * between two Vulkan instances in separate processes.  People who want to
    * share memory need to also check the device UUID (below) so all this
    * needs to be is the build-id.
    */
   memcpy(device->driver_uuid, build_id_data(note), VK_UUID_SIZE);

   /* The device UUID uniquely identifies the given device within the machine.
    * Since we never have more than one device, this doesn't need to be a real
    * UUID.  However, on the off-chance that someone tries to use this to
    * cache pre-tiled images or something of the like, we use the PCI ID and
    * some bits of ISL info to ensure that this is safe.
    */
   _mesa_sha1_init(&sha1_ctx);
   _mesa_sha1_update(&sha1_ctx, &device->chipset_id,
                     sizeof(device->chipset_id));
#if 0
   _mesa_sha1_update(&sha1_ctx, &device->isl_dev.has_bit6_swizzling,
                     sizeof(device->isl_dev.has_bit6_swizzling));
#endif
   _mesa_sha1_final(&sha1_ctx, sha1);
   memcpy(device->device_uuid, sha1, VK_UUID_SIZE);

   return VK_SUCCESS;
}

static VkResult
v3dvk_physical_device_init(struct v3dvk_physical_device *device,
                         struct v3dvk_instance *instance,
                         drmDevicePtr drm_device)
{
   const char *primary_path = drm_device->nodes[DRM_NODE_PRIMARY];
   const char *path = drm_device->nodes[DRM_NODE_RENDER];
   VkResult result;
   int fd;
   int master_fd = -1;

   fd = open(path, O_RDWR | O_CLOEXEC);
   if (fd < 0)
      return vk_error(VK_ERROR_INCOMPATIBLE_DRIVER);

   device->_loader_data.loaderMagic = ICD_LOADER_MAGIC;
   device->instance = instance;

   assert(strlen(path) < ARRAY_SIZE(device->path));
   snprintf(device->path, ARRAY_SIZE(device->path), "%s", path);

   device->chipset_id = v3d_get_version(fd);
   if (!device->chipset_id) {
      result = vk_error(VK_ERROR_INCOMPATIBLE_DRIVER);
      goto fail;
   }

   device->pci_info.domain = drm_device->businfo.pci->domain;
   device->pci_info.bus = drm_device->businfo.pci->bus;
   device->pci_info.device = drm_device->businfo.pci->dev;
   device->pci_info.function = drm_device->businfo.pci->func;

   device->name = gen_get_device_name(device->chipset_id);
   if (!gen_get_device_info(device->chipset_id, &device->info)) {
      result = vk_error(VK_ERROR_INCOMPATIBLE_DRIVER);
      goto fail;
   }

   if (true) {
      /* Videocore 6 fully supported */
   } else {
      result = vk_errorf(device->instance, device,
                         VK_ERROR_INCOMPATIBLE_DRIVER,
                         "Vulkan not yet supported on %s", device->name);
      goto fail;
   }

   result = v3dvk_physical_device_init_uuids(device);
   if (result != VK_SUCCESS)
      goto fail;
#if 0
   if (instance->enabled_extensions.KHR_display) {
#else
   if (0) {
#endif
      master_fd = open(primary_path, O_RDWR | O_CLOEXEC);
#if 0
      if (master_fd >= 0) {
         /* prod the device with a GETPARAM call which will fail if
          * we don't have permission to even render on this device
          */
         if (anv_gem_get_param(master_fd, I915_PARAM_CHIPSET_ID) == 0) {
            close(master_fd);
            master_fd = -1;
         }
      }
#endif
   }

   device->master_fd = master_fd;

   result = v3dvk_init_wsi(device);
   if (result != VK_SUCCESS) {
      goto fail;
   }

   device->local_fd = fd;

   return VK_SUCCESS;

fail:
   close(fd);
   if (master_fd != -1)
      close(master_fd);
   return result;
}

void
v3dvk_physical_device_finish(struct v3dvk_physical_device *device)
{
	// TODO: implement
}
