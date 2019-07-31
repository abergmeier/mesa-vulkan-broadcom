/*
 * Copyright © 2015 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#ifndef V3DVK_INSTANCE_H
#define V3DVK_INSTANCE_H

#include <vulkan/vulkan.h>

#include "v3dvk_entrypoints.h"
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

    struct v3dvk_instance_extension_table       enabled_extensions;

    struct v3dvk_instance_dispatch_table        dispatch;
    struct v3dvk_device_dispatch_table          device_dispatch;

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
