
#include <vulkan/vulkan.h>
#include "common.h"
#include "device.h"
#include "vk_alloc.h"
#include "v3dvk_entrypoints.h"
#include "v3dvk_error.h"
#include "v3dvk_semaphore.h"

VkResult
v3dvk_CreateSemaphore(VkDevice _device,
                      const VkSemaphoreCreateInfo *pCreateInfo,
                      const VkAllocationCallbacks *pAllocator,
                      VkSemaphore *pSemaphore)
{
   V3DVK_FROM_HANDLE(v3dvk_device, device, _device);

   struct v3dvk_semaphore *sem =
      vk_alloc2(&device->alloc, pAllocator, sizeof(*sem), 8,
                VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
   if (!sem)
      return v3dvk_error(device->instance, VK_ERROR_OUT_OF_HOST_MEMORY);

   *pSemaphore = v3dvk_semaphore_to_handle(sem);
   return VK_SUCCESS;
}

void
v3dvk_DestroySemaphore(VkDevice _device,
                       VkSemaphore _semaphore,
                       const VkAllocationCallbacks *pAllocator)
{
   V3DVK_FROM_HANDLE(v3dvk_device, device, _device);
   V3DVK_FROM_HANDLE(v3dvk_semaphore, sem, _semaphore);
   if (!_semaphore)
      return;

   vk_free2(&device->alloc, pAllocator, sem);
}
