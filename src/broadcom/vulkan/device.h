#ifndef V3DVK_DEVICE_H
#define V3DVK_DEVICE_H

#include <vulkan/vk_icd.h>
#include <vulkan/vulkan.h>

#include "common/v3d_device_info.h"
#include "util/macros.h"
#include "v3dvk_entrypoints.h"

struct v3dvk_device {
    VK_LOADER_DATA                              _loader_data;

    VkAllocationCallbacks                       alloc;

    struct v3dvk_instance *                     instance;
    struct v3d_device_info                      info;
    struct v3dvk_device_dispatch_table          dispatch;

    bool                                        _lost;
};

VkResult _v3dvk_device_set_lost(struct v3dvk_device *device,
                                const char *file, int line,
                                const char *msg, ...);
#define v3dvk_device_set_lost(dev, ...) \
   _v3dvk_device_set_lost(dev, __FILE__, __LINE__, __VA_ARGS__)

static inline bool
v3dvk_device_is_lost(struct v3dvk_device *device)
{
   return unlikely(device->_lost);
}

VkResult v3dvk_device_query_status(struct v3dvk_device *device);

#endif // V3DVK_DEVICE_H
