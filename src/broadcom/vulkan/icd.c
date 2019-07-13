#include <string.h>
#include <vulkan/vk_icd.h>

#include "common.h"
#include "util/macros.h"
#include "instance.h"

static
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
#if 0
   LOOKUP_V3DVK_ENTRYPOINT(EnumerateInstanceExtensionProperties);
   LOOKUP_V3DVK_ENTRYPOINT(EnumerateInstanceLayerProperties);
   LOOKUP_V3DVK_ENTRYPOINT(EnumerateInstanceVersion);
#endif
   LOOKUP_V3DVK_ENTRYPOINT(CreateInstance);

#undef LOOKUP_ANV_ENTRYPOINT
   if (instance == NULL)
      return NULL;

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
