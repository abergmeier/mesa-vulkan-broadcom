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

#ifndef V3DVK_BATCH_H
#define V3DVK_BATCH_H

#include <stdbool.h>
#include <vulkan/vulkan.h>

struct v3dvk_batch {
   const VkAllocationCallbacks *                alloc;

   void *                                       start;
   void *                                       end;
   void *                                       next;
#if 0
   struct anv_reloc_list *                      relocs;

   /* This callback is called (with the associated user data) in the event
    * that the batch runs out of space.
    */
   VkResult (*extend_cb)(struct anv_batch *, void *);
   void *                                       user_data;
#endif
   /**
    * Current error status of the command buffer. Used to track inconsistent
    * or incomplete command buffer states that are the consequence of run-time
    * errors such as out of memory scenarios. We want to track this in the
    * batch because the command buffer object is not visible to some parts
    * of the driver.
    */
   VkResult                                     status;
};

static inline bool
v3dvk_batch_has_error(struct v3dvk_batch *batch)
{
   return batch->status != VK_SUCCESS;
}

/* Wrapper macros needed to work around preprocessor argument issues.  In
 * particular, arguments don't get pre-evaluated if they are concatenated.
 * This means that, if you pass GENX(3DSTATE_PS) into the emit macro, the
 * GENX macro won't get evaluated if the emit macro contains "cmd ## foo".
 * We can work around this easily enough with these helpers.
 */
#define __v3dvk_cmd_length(cmd) cmd ## _length
#define __v3dvk_cmd_length_bias(cmd) cmd ## _length_bias
#define __v3dvk_cmd_header(cmd) cmd ## _header
#define __v3dvk_cmd_pack(cmd) cmd ## _pack

#define cl_emit(cl, packet, name)                                \
        for (struct cl_packet_struct(packet) name = {            \
                cl_packet_header(packet)                         \
        },                                                       \
        *_loop_terminate = &name;                                \
        __builtin_expect(_loop_terminate != NULL, 1);            \
        ({                                                       \
                struct v3d_cl_out *cl_out = cl_start(cl);        \
                cl_packet_pack(packet)(cl, (uint8_t *)cl_out, &name); \
                cl_advance(&cl_out, cl_packet_length(packet));   \
                cl_end(cl, cl_out);                              \
                _loop_terminate = NULL;                          \
        }))                                                      \

#define v3dvk_batch_emit(batch, cmd, name)                              \
   for (struct cmd name = { __v3dvk_cmd_header(cmd) },                  \
        *_dst = v3dvk_batch_emit_dwords(batch, __v3dvk_cmd_length(cmd));\
        __builtin_expect(_dst != NULL, 1);                              \
        ({ __v3dvk_cmd_pack(cmd)(batch, _dst, &name);                   \
           VG(VALGRIND_CHECK_MEM_IS_DEFINED(_dst, __v3dvk_cmd_length(cmd) * 4)); \
           _dst = NULL;                                                 \
         }))

#endif
