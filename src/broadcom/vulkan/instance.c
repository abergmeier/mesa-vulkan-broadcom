#include <vulkan/vk_icd.h>

#include "common.h"
#include "compiler/glsl_types.h"
#include "device.h"
#include "instance.h"
#include "util/strtod.h"
#include "vk_alloc.h"
#include "vk_debug_report.h"
#include "valgrind.h"

static void *
default_alloc_func(void *pUserData, size_t size, size_t align,
                   VkSystemAllocationScope allocationScope)
{
   return malloc(size);
}

static void *
default_realloc_func(void *pUserData, void *pOriginal, size_t size,
                     size_t align, VkSystemAllocationScope allocationScope)
{
   return realloc(pOriginal, size);
}

static void
default_free_func(void *pUserData, void *pMemory)
{
   free(pMemory);
}

static const VkAllocationCallbacks default_alloc = {
   .pUserData = NULL,
   .pfnAllocation = default_alloc_func,
   .pfnReallocation = default_realloc_func,
   .pfnFree = default_free_func,
};

/*
 * https://www.khronos.org/registry/vulkan/specs/1.1-extensions/html/vkspec.html#vkCreateInstance
 * There is no global state in Vulkan and all per-application state is stored in a VkInstance object. Creating a VkInstance object initializes the Vulkan library
 * vkCreateInstance verifies that the requested layers exist. If not, vkCreateInstance will return VK_ERROR_LAYER_NOT_PRESENT. Next vkCreateInstance verifies that
 * the requested extensions are supported (e.g. in the implementation or in any enabled instance layer) and if any requested extension is not supported,
 * vkCreateInstance must return VK_ERROR_EXTENSION_NOT_PRESENT. After verifying and enabling the instance layers and extensions the VkInstance object is
 * created and returned to the application.
 */
VkResult v3dvk_CreateInstance(
    const VkInstanceCreateInfo*                 pCreateInfo,
    const VkAllocationCallbacks*                pAllocator,
    VkInstance*                                 pInstance)
{
   struct v3dvk_instance *instance;
   VkResult result;

   assert(pCreateInfo->sType == VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO);
   //TODO: Support extensions
   assert(pCreateInfo->enabledExtensionCount == 0);

   instance = vk_alloc2(&default_alloc, pAllocator, sizeof(*instance), 8,
                         VK_SYSTEM_ALLOCATION_SCOPE_INSTANCE);
   if (!instance)
      return vk_error(VK_ERROR_OUT_OF_HOST_MEMORY);

   instance->_loader_data.loaderMagic = ICD_LOADER_MAGIC;

   if (pAllocator)
      instance->alloc = *pAllocator;
   else
      instance->alloc = default_alloc;

   instance->app_info = (struct v3dvk_app_info) { .api_version = 0 };
   if (pCreateInfo->pApplicationInfo) {
      const VkApplicationInfo *app = pCreateInfo->pApplicationInfo;

      instance->app_info.app_name =
         vk_strdup(&instance->alloc, app->pApplicationName,
                   VK_SYSTEM_ALLOCATION_SCOPE_INSTANCE);
      instance->app_info.app_version = app->applicationVersion;

      instance->app_info.engine_name =
         vk_strdup(&instance->alloc, app->pEngineName,
                   VK_SYSTEM_ALLOCATION_SCOPE_INSTANCE);
      instance->app_info.engine_version = app->engineVersion;

      instance->app_info.api_version = app->apiVersion;

   }
   if (instance->app_info.api_version == 0)
      instance->app_info.api_version = VK_API_VERSION_1_0;

   instance->physicalDeviceCount = -1;

   result = vk_debug_report_instance_init(&instance->debug_report_callbacks);
   if (result != VK_SUCCESS) {
      vk_free2(&default_alloc, pAllocator, instance);
      return vk_error(result);
   }

   // TODO: Implement pipeline cache
   instance->pipeline_cache_enabled = false;

   _mesa_locale_init();
   glsl_type_singleton_init_or_ref();

   VG(VALGRIND_CREATE_MEMPOOL(instance, 0, false));

   *pInstance = v3dvk_instance_to_handle(instance);

   return VK_SUCCESS;
}

void v3dvk_DestroyInstance(
    VkInstance                                  _instance,
    const VkAllocationCallbacks*                pAllocator)
{
   V3DVK_FROM_HANDLE(v3dvk_instance, instance, _instance);

   if (!instance)
      return;

   if (instance->physicalDeviceCount > 0) {
      // We support at most one physical device.
      assert(instance->physicalDeviceCount == 1);
      v3dvk_physical_device_finish(&instance->physicalDevice);
   }

   vk_free(&instance->alloc, (char *)instance->app_info.app_name);
   vk_free(&instance->alloc, (char *)instance->app_info.engine_name);

   VG(VALGRIND_DESTROY_MEMPOOL(instance));

   vk_debug_report_instance_destroy(&instance->debug_report_callbacks);

   glsl_type_singleton_decref();
   _mesa_locale_fini();

   vk_free(&instance->alloc, instance);
}
