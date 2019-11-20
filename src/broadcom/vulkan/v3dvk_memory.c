
#include <vulkan/vulkan_core.h>
#include "vk_alloc.h"
#include "common.h"
#include "device.h"
#include "v3dvk_bo.h"
#include "v3dvk_error.h"
#include "v3dvk_memory.h"

static VkResult
v3dvk_alloc_memory(struct v3dvk_device *device,
                const VkMemoryAllocateInfo *pAllocateInfo,
                const VkAllocationCallbacks *pAllocator,
                VkDeviceMemory *pMem)
{
   struct v3dvk_device_memory *mem;
   VkResult result;

   assert(pAllocateInfo->sType == VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO);

   mem = vk_alloc2(&device->alloc, pAllocator, sizeof(*mem), 8,
                   VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
   if (mem == NULL)
      return v3dvk_error(device->instance, VK_ERROR_OUT_OF_HOST_MEMORY);
#if 0
   const VkImportMemoryFdInfoKHR *fd_info =
      vk_find_struct_const(pAllocateInfo->pNext, IMPORT_MEMORY_FD_INFO_KHR);
   if (fd_info && !fd_info->handleType)
      fd_info = NULL;

   if (fd_info) {
      assert(fd_info->handleType ==
                VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT ||
             fd_info->handleType ==
                VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT);

      /*
       * TODO Importing the same fd twice gives us the same handle without
       * reference counting.  We need to maintain a per-instance handle-to-bo
       * table and add reference count to tu_bo.
       */
      result = tu_bo_init_dmabuf(device, &mem->bo,
                                 pAllocateInfo->allocationSize, fd_info->fd);
      if (result == VK_SUCCESS) {
         /* take ownership and close the fd */
         close(fd_info->fd);
      }
   } else {
#endif
   if (pAllocateInfo->allocationSize == 0) {
      result =
         v3dvk_bo_init_new(device, &mem->bo, 1);
    } else {
      result =
         v3dvk_bo_init_new(device, &mem->bo, pAllocateInfo->allocationSize);
    }
#if 0
   }
#endif

   if (result != VK_SUCCESS) {
      vk_free2(&device->alloc, pAllocator, mem);
      return result;
   }

   mem->size = pAllocateInfo->allocationSize;
   mem->type_index = pAllocateInfo->memoryTypeIndex;
#if 0
   mem->map = NULL;
   mem->user_ptr = NULL;
#endif
   *pMem = v3dvk_device_memory_to_handle(mem);

   return VK_SUCCESS;
}

VkResult
v3dvk_AllocateMemory(VkDevice _device,
                  const VkMemoryAllocateInfo *pAllocateInfo,
                  const VkAllocationCallbacks *pAllocator,
                  VkDeviceMemory *pMem)
{
   V3DVK_FROM_HANDLE(v3dvk_device, device, _device);
   return v3dvk_alloc_memory(device, pAllocateInfo, pAllocator, pMem);
}

void
v3dvk_FreeMemory(VkDevice _device,
              VkDeviceMemory _mem,
              const VkAllocationCallbacks *pAllocator)
{
   V3DVK_FROM_HANDLE(v3dvk_device, device, _device);
   V3DVK_FROM_HANDLE(v3dvk_device_memory, mem, _mem);

   if (mem == NULL)
      return;

   v3dvk_bo_finish(device, &mem->bo);
   vk_free2(&device->alloc, pAllocator, mem);
}
