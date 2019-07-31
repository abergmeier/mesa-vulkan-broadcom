#include <string.h>
#include <vulkan/vk_icd.h>

#include "common.h"
#include "util/macros.h"
#include "instance.h"
#include "v3dvk_entrypoints.h"

PFN_vkVoidFunction v3dvk_GetInstanceProcAddr(
    VkInstance                                  _instance,
    const char*                                 pName)
{
   V3DVK_FROM_HANDLE(v3dvk_instance, instance, _instance);

   /* The Vulkan 1.0 spec for vkGetInstanceProcAddr has a table of exactly
    * when we have to return valid function pointers, NULL, or it's left
    * undefined.  See the table for exact details.
    */
   if (pName == NULL)
      return NULL;

#define LOOKUP_V3DVK_ENTRYPOINT(entrypoint) \
   if (strcmp(pName, "vk" #entrypoint) == 0) \
      return (PFN_vkVoidFunction)v3dvk_##entrypoint
   LOOKUP_V3DVK_ENTRYPOINT(EnumerateInstanceExtensionProperties);
#if 0
   LOOKUP_V3DVK_ENTRYPOINT(EnumerateInstanceLayerProperties);
#endif
   LOOKUP_V3DVK_ENTRYPOINT(EnumerateInstanceVersion);
   LOOKUP_V3DVK_ENTRYPOINT(CreateInstance);

#undef LOOKUP_V3DVK_ENTRYPOINT
   if (instance == NULL)
      return NULL;

   int idx = v3dvk_get_instance_entrypoint_index(pName);
   if (idx >= 0)
      return instance->dispatch.entrypoints[idx];

   idx = v3dvk_get_device_entrypoint_index(pName);
   if (idx >= 0)
      return instance->device_dispatch.entrypoints[idx];

   // TODO: Implement
   return NULL;
}

/* With version 1+ of the loader interface the ICD should expose
 * vk_icdGetInstanceProcAddr to work around certain LD_PRELOAD issues seen in apps.
 */
PUBLIC
VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vk_icdGetInstanceProcAddr(
    VkInstance                                  instance,
    const char*                                 pName);

PUBLIC
VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vk_icdGetInstanceProcAddr(
    VkInstance                                  instance,
    const char*                                 pName)
{
   return v3dvk_GetInstanceProcAddr(instance, pName);
}
