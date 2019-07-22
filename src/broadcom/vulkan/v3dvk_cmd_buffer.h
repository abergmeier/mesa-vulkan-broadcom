
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

#ifndef V3DVK_CMD_BUFFER_H
#define V3DVK_CMD_BUFFER_H

#include <vulkan/vk_icd.h>
#include <vulkan/vulkan.h>
#include "util/list.h"

struct v3dvk_cmd_pool;
struct v3dvk_device;

struct v3dvk_cmd_buffer {
   VK_LOADER_DATA                               _loader_data;

   struct v3dvk_device *                          device;

   struct v3dvk_cmd_pool *                        pool;
   struct list_head                             pool_link;
#if 0
   struct v3dvk_batch                           batch;

   /* Fields required for the actual chain of anv_batch_bo's.
    *
    * These fields are initialized by anv_cmd_buffer_init_batch_bo_chain().
    */
   struct list_head                             batch_bos;
   enum v3dvk_cmd_buffer_exec_mode              exec_mode;

   /* A vector of anv_batch_bo pointers for every batch or surface buffer
    * referenced by this command buffer
    *
    * initialized by anv_cmd_buffer_init_batch_bo_chain()
    */
   struct u_vector                            seen_bbos;

   /* A vector of int32_t's for every block of binding tables.
    *
    * initialized by anv_cmd_buffer_init_batch_bo_chain()
    */
   struct u_vector                              bt_block_states;
   uint32_t                                     bt_next;

   struct anv_reloc_list                        surface_relocs;
   /** Last seen surface state block pool center bo offset */
   uint32_t                                     last_ss_pool_center;

   /* Serial for tracking buffer completion */
   uint32_t                                     serial;

   /* Stream objects for storing temporary data */
   struct anv_state_stream                      surface_state_stream;
   struct anv_state_stream                      dynamic_state_stream;

   VkCommandBufferUsageFlags                    usage_flags;
   VkCommandBufferLevel                         level;

   struct anv_cmd_state                         state;
#endif
};

void
v3dvk_cmd_buffer_destroy(struct v3dvk_cmd_buffer *cmd_buffer);

#endif // V3DVK_CMD_BUFFER_H
