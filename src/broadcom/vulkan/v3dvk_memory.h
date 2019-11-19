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

#ifndef V3DVK_MEMORY_H
#define V3DVK_MEMORY_H

#include <vulkan/vulkan.h>
#include "v3dvk_bo.h"

struct v3dvk_memory_type {
   /* Standard bits passed on to the client */
   VkMemoryPropertyFlags   propertyFlags;
   uint32_t                heapIndex;

   /* Driver-internal book-keeping */
#if 0
   VkBufferUsageFlags      valid_buffer_usage;
#endif
};

struct v3dvk_memory_heap {
   /* Standard bits passed on to the client */
   VkDeviceSize      size;
   VkMemoryHeapFlags flags;

   /* Driver-internal book-keeping */
#if 0
   uint64_t          vma_start;
   uint64_t          vma_size;
   bool              supports_48bit_addresses;
#endif
   VkDeviceSize      used;
};

struct v3dvk_device_memory
{
   struct v3dvk_bo bo;
   VkDeviceSize size;
#if 0
   /* for dedicated allocations */
   struct tu_image *image;
   struct tu_buffer *buffer;
#endif
   uint32_t type_index;
#if 0
   void *map;
   void *user_ptr;
#endif
};

#endif
