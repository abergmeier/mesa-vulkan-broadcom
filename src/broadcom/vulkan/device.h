#ifndef V3DVK_DEVICE_H
#define V3DVK_DEVICE_H

#include <vulkan/vk_icd.h>
#include <vulkan/vulkan.h>

#include "common/v3d_device_info.h"

#include "v3dvk_entrypoints.h"

struct v3dvk_device {
    VK_LOADER_DATA                              _loader_data;

    VkAllocationCallbacks                       alloc;

    struct v3dvk_instance *                     instance;
    struct v3d_device_info                      info;
    struct v3dvk_device_dispatch_table          dispatch;
};

#endif // V3DVK_DEVICE_H
