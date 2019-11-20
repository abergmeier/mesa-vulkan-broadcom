
#include "common.h"
#include "device.h"
#include "vk_alloc.h"
#include "v3dvk_entrypoints.h"
#include "v3dvk_error.h"
#include "v3dvk_query.h"

VkResult
v3dvk_CreateQueryPool(VkDevice _device,
                      const VkQueryPoolCreateInfo *pCreateInfo,
                      const VkAllocationCallbacks *pAllocator,
                      VkQueryPool *pQueryPool)
{
   V3DVK_FROM_HANDLE(v3dvk_device, device, _device);
   struct v3dvk_query_pool *pool =
      vk_alloc2(&device->alloc, pAllocator, sizeof(*pool), 8,
                VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);

   if (!pool)
      return v3dvk_error(device->instance, VK_ERROR_OUT_OF_HOST_MEMORY);

   *pQueryPool = v3dvk_query_pool_to_handle(pool);
   return VK_SUCCESS;
}

void
v3dvk_DestroyQueryPool(VkDevice _device,
                       VkQueryPool _pool,
                       const VkAllocationCallbacks *pAllocator)
{
   V3DVK_FROM_HANDLE(v3dvk_device, device, _device);
   V3DVK_FROM_HANDLE(v3dvk_query_pool, pool, _pool);

   if (!pool)
      return;

   vk_free2(&device->alloc, pAllocator, pool);
}
