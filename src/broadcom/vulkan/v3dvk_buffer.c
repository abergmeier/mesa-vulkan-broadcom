/*
 * Copyright © 2016 Red Hat.
 * Copyright © 2016 Bas Nieuwenhuizen
 *
 * based in part on anv driver which is:
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
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include "util/u_math.h"
#include "common.h"
#include "v3dvk_buffer.h"
#include "v3dvk_entrypoints.h"
#include "v3dvk_memory.h"

void
v3dvk_GetBufferMemoryRequirements(VkDevice _device,
                                  VkBuffer _buffer,
                                  VkMemoryRequirements *pMemoryRequirements)
{
   V3DVK_FROM_HANDLE(v3dvk_buffer, buffer, _buffer);

   pMemoryRequirements->memoryTypeBits = 1;
   pMemoryRequirements->alignment = 16;
   pMemoryRequirements->size =
      align64(buffer->size, pMemoryRequirements->alignment);
}

void
v3dvk_GetBufferMemoryRequirements2(
   VkDevice device,
   const VkBufferMemoryRequirementsInfo2 *pInfo,
   VkMemoryRequirements2 *pMemoryRequirements)
{
   v3dvk_GetBufferMemoryRequirements(device, pInfo->buffer,
                                     &pMemoryRequirements->memoryRequirements);
}

VkResult
v3dvk_BindBufferMemory2(VkDevice device,
                        uint32_t bindInfoCount,
                        const VkBindBufferMemoryInfo *pBindInfos)
{
   for (uint32_t i = 0; i < bindInfoCount; ++i) {
      V3DVK_FROM_HANDLE(v3dvk_device_memory, mem, pBindInfos[i].memory);
      V3DVK_FROM_HANDLE(v3dvk_buffer, buffer, pBindInfos[i].buffer);

      if (mem) {
         buffer->bo = &mem->bo;
         buffer->bo_offset = pBindInfos[i].memoryOffset;
      } else {
         buffer->bo = NULL;
      }
   }
   return VK_SUCCESS;
}

VkResult
v3dvk_BindBufferMemory(VkDevice device,
                       VkBuffer buffer,
                       VkDeviceMemory memory,
                       VkDeviceSize memoryOffset)
{
   const VkBindBufferMemoryInfo info = {
      .sType = VK_STRUCTURE_TYPE_BIND_BUFFER_MEMORY_INFO,
      .buffer = buffer,
      .memory = memory,
      .memoryOffset = memoryOffset
   };

   return v3dvk_BindBufferMemory2(device, 1, &info);
}
