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

#ifndef V3DVK_DEVICE_H
#define V3DVK_DEVICE_H

#include <pthread.h>

#include <vulkan/vk_icd.h>
#include <vulkan/vulkan.h>

#include "common/v3d_device_info.h"
#include "util/macros.h"
#include "v3dvk_entrypoints.h"
#include "v3dvk_queue.h"

struct v3d_compiler;

struct v3dvk_device {
    VK_LOADER_DATA                              _loader_data;

    VkAllocationCallbacks                       alloc;

    struct v3dvk_instance *                     instance;
    struct v3d_device_info                      info;
    const struct v3d_compiler *                 compiler;
    int                                         fd;
    struct v3dvk_device_extension_table         enabled_extensions;
    struct v3dvk_device_dispatch_table          dispatch;

    struct v3dvk_queue* queues[V3DVK_MAX_QUEUE_FAMILIES];
    int queue_count[V3DVK_MAX_QUEUE_FAMILIES];

    pthread_mutex_t                             mutex;
    pthread_cond_t                              queue_submit;
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
