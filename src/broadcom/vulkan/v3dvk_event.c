
#include "common.h"
#include "device.h"
#include "vk_alloc.h"
#include "v3dvk_error.h"
#include "v3dvk_event.h"

VkResult
v3dvk_CreateEvent(VkDevice _device,
                  const VkEventCreateInfo *pCreateInfo,
                  const VkAllocationCallbacks *pAllocator,
                  VkEvent *pEvent)
{
   V3DVK_FROM_HANDLE(v3dvk_device, device, _device);
   struct v3dvk_event *event =
      vk_alloc2(&device->alloc, pAllocator, sizeof(*event), 8,
                VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);

   if (!event)
      return v3dvk_error(device->instance, VK_ERROR_OUT_OF_HOST_MEMORY);

   *pEvent = v3dvk_event_to_handle(event);

   return VK_SUCCESS;
}

void
v3dvk_DestroyEvent(VkDevice _device,
                   VkEvent _event,
                   const VkAllocationCallbacks *pAllocator)
{
   V3DVK_FROM_HANDLE(v3dvk_device, device, _device);
   V3DVK_FROM_HANDLE(v3dvk_event, event, _event);

   if (!event)
      return;
   vk_free2(&device->alloc, pAllocator, event);
}
