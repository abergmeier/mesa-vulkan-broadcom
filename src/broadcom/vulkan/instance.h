#ifndef V3DVK_INSTANCE_H
#define V3DVK_INSTANCE_H

#include <vulkan/vulkan.h>

#include "v3dvk_physical_device.h"
#include "vulkan/util/vk_debug_report.h"

struct v3dvk_app_info {
   const char*        app_name;
   uint32_t           app_version;
   const char*        engine_name;
   uint32_t           engine_version;
   uint32_t           api_version;
};

struct v3dvk_instance {
    VK_LOADER_DATA                              _loader_data;

    VkAllocationCallbacks                       alloc;

    struct v3dvk_app_info                       app_info;
/*
    struct anv_instance_extension_table         enabled_extensions;
    struct anv_instance_dispatch_table          dispatch;
    struct anv_device_dispatch_table            device_dispatch;
*/
    int                                         physicalDeviceCount;
    struct v3dvk_physical_device                physicalDevice;

    bool                                        pipeline_cache_enabled;

    struct vk_debug_report_instance             debug_report_callbacks;
};

VkResult v3dvk_CreateInstance(
    const VkInstanceCreateInfo*                 pCreateInfo,
    const VkAllocationCallbacks*                pAllocator,
    VkInstance*                                 pInstance);

void v3dvk_DestroyInstance(
    VkInstance                                  _instance,
    const VkAllocationCallbacks*                pAllocator);

#endif // V3DVK_INSTANCE_H
