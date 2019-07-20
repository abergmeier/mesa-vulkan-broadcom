#ifndef V3DVK_WSI_H
#define V3DVK_WSI_H

#include <vulkan/vulkan.h>

struct v3dvk_physical_device;

VkResult v3dvk_init_wsi(struct v3dvk_physical_device *physical_device);
void v3dvk_finish_wsi(struct v3dvk_physical_device *physical_device);

#endif // V3DVK_WSI_H
