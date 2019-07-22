
#include "common.h"
#include "device.h"
#include "v3dvk_cmd_buffer.h"
#include "v3dvk_cmd_pool.h"
#include "vulkan/util/vk_alloc.h"

VkResult v3dvk_CreateCommandPool(
    VkDevice                                    _device,
    const VkCommandPoolCreateInfo*              pCreateInfo,
    const VkAllocationCallbacks*                pAllocator,
    VkCommandPool*                              pCmdPool)
{
   V3DVK_FROM_HANDLE(v3dvk_device, device, _device);
   struct v3dvk_cmd_pool *pool;

   pool = vk_alloc2(&device->alloc, pAllocator, sizeof(*pool), 8,
                     VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
   if (pool == NULL)
      return vk_error(VK_ERROR_OUT_OF_HOST_MEMORY);

   if (pAllocator)
      pool->alloc = *pAllocator;
   else
      pool->alloc = device->alloc;

   list_inithead(&pool->cmd_buffers);

   *pCmdPool = v3dvk_cmd_pool_to_handle(pool);

   return VK_SUCCESS;
}

void v3dvk_DestroyCommandPool(
    VkDevice                                    _device,
    VkCommandPool                               commandPool,
    const VkAllocationCallbacks*                pAllocator)
{
   V3DVK_FROM_HANDLE(v3dvk_device, device, _device);
   V3DVK_FROM_HANDLE(v3dvk_cmd_pool, pool, commandPool);

   if (!pool)
      return;

   list_for_each_entry_safe(struct v3dvk_cmd_buffer, cmd_buffer,
                            &pool->cmd_buffers, pool_link) {
      v3dvk_cmd_buffer_destroy(cmd_buffer);
   }

   vk_free2(&device->alloc, pAllocator, pool);
}
