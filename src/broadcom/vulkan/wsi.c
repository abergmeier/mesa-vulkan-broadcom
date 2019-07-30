#include <vulkan/vulkan.h>

#include "common.h"
#include "device.h"
#include "instance.h"
#include "v3dvk_physical_device.h"
#include "wsi.h"

static PFN_vkVoidFunction
v3dvk_wsi_proc_addr(VkPhysicalDevice physicalDevice, const char *pName)
{
   V3DVK_FROM_HANDLE(v3dvk_physical_device, physical_device, physicalDevice);
   return v3dvk_lookup_entrypoint(&physical_device->info, pName);
}

VkResult
v3dvk_init_wsi(struct v3dvk_physical_device *physical_device)
{
   VkResult result;

   result = wsi_device_init(&physical_device->wsi_device,
                            v3dvk_physical_device_to_handle(physical_device),
                            v3dvk_wsi_proc_addr,
                            &physical_device->instance->alloc,
                            physical_device->master_fd,
                            NULL);
   if (result != VK_SUCCESS)
      return result;

   physical_device->wsi_device.supports_modifiers = false;
#if 0
   physical_device->wsi_device.image_get_modifier = anv_wsi_image_get_modifier;
#endif
   return VK_SUCCESS;
}

void
v3dvk_finish_wsi(struct v3dvk_physical_device *physical_device)
{
   wsi_device_finish(&physical_device->wsi_device,
                     &physical_device->instance->alloc);
}
