
#include "common.h"
#include "device.h"
#include "vk_alloc.h"
#include "v3dvk_error.h"
#include "v3dvk_shader.h"
#include "util/mesa-sha1.h"

VkResult
v3dvk_CreateShaderModule(VkDevice _device,
                         const VkShaderModuleCreateInfo *pCreateInfo,
                         const VkAllocationCallbacks *pAllocator,
                         VkShaderModule *pShaderModule)
{
   V3DVK_FROM_HANDLE(v3dvk_device, device, _device);
   struct v3dvk_shader_module *module;

   assert(pCreateInfo->sType == VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO);
   assert(pCreateInfo->flags == 0);
   assert(pCreateInfo->codeSize % 4 == 0);

   module = vk_alloc2(&device->alloc, pAllocator,
                      sizeof(*module) + pCreateInfo->codeSize, 8,
                      VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
   if (module == NULL)
      return v3dvk_error(device->instance, VK_ERROR_OUT_OF_HOST_MEMORY);

   module->code_size = pCreateInfo->codeSize;
   memcpy(module->code, pCreateInfo->pCode, pCreateInfo->codeSize);

   _mesa_sha1_compute(module->code, module->code_size, module->sha1);

   *pShaderModule = v3dvk_shader_module_to_handle(module);

   return VK_SUCCESS;
}

void
v3dvk_DestroyShaderModule(VkDevice _device,
                          VkShaderModule _module,
                          const VkAllocationCallbacks *pAllocator)
{
   V3DVK_FROM_HANDLE(v3dvk_device, device, _device);
   V3DVK_FROM_HANDLE(v3dvk_shader_module, module, _module);

   if (!module)
      return;

   vk_free2(&device->alloc, pAllocator, module);
}
