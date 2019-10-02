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

#include "common.h"
#include "v3dvk_buffer.h"
#include "v3dvk_cmd_buffer.h"
#include "v3dvk_cmd_pool.h"
#include "v3dvk_entrypoints.h"
#include "vulkan/util/vk_alloc.h"

/* TODO: These are taken from GLES.  We should check the Vulkan spec */
const struct v3dvk_dynamic_state default_dynamic_state = {
   .viewport = {
      .count = 0,
   },
   .scissor = {
      .count = 0,
   },
   .line_width = 1.0f,
   .depth_bias = {
      .bias = 0.0f,
      .clamp = 0.0f,
      .slope = 0.0f,
   },
   .blend_constants = { 0.0f, 0.0f, 0.0f, 0.0f },
   .depth_bounds = {
      .min = 0.0f,
      .max = 1.0f,
   },
   .stencil_compare_mask = {
      .front = ~0u,
      .back = ~0u,
   },
   .stencil_write_mask = {
      .front = ~0u,
      .back = ~0u,
   },
   .stencil_reference = {
      .front = 0u,
      .back = 0u,
   },
};

static void
v3dvk_cmd_state_init(struct v3dvk_cmd_buffer *cmd_buffer)
{
   struct v3dvk_cmd_state *state = &cmd_buffer->state;

   memset(state, 0, sizeof(*state));

   state->current_pipeline = UINT32_MAX;
#if 0
   state->restart_index = UINT32_MAX;
#endif
   state->gfx.dynamic = default_dynamic_state;
}

static void
v3dvk_cmd_state_finish(struct v3dvk_cmd_buffer *cmd_buffer)
{
   struct v3dvk_cmd_state *state = &cmd_buffer->state;
#if 0
   v3dvk_cmd_pipeline_state_finish(cmd_buffer, &state->gfx.base);
   v3dvk_cmd_pipeline_state_finish(cmd_buffer, &state->compute.base);
#endif
   vk_free(&cmd_buffer->pool->alloc, state->attachments);
}

static void
v3dvk_cmd_state_reset(struct v3dvk_cmd_buffer *cmd_buffer)
{
   v3dvk_cmd_state_finish(cmd_buffer);
   v3dvk_cmd_state_init(cmd_buffer);
}

void
v3dvk_cmd_buffer_destroy(struct v3dvk_cmd_buffer *cmd_buffer)
{
   list_del(&cmd_buffer->pool_link);
#if 0
   anv_cmd_buffer_fini_batch_bo_chain(cmd_buffer);

   anv_state_stream_finish(&cmd_buffer->surface_state_stream);
   anv_state_stream_finish(&cmd_buffer->dynamic_state_stream);

   anv_cmd_state_finish(cmd_buffer);
#endif
   vk_free(&cmd_buffer->pool->alloc, cmd_buffer);
}

VkResult
v3dvk_cmd_buffer_reset(struct v3dvk_cmd_buffer *cmd_buffer)
{
   cmd_buffer->usage_flags = 0;
#if 0
   anv_cmd_buffer_reset_batch_bo_chain(cmd_buffer);
#endif
   v3dvk_cmd_state_reset(cmd_buffer);
#if 0
   anv_state_stream_finish(&cmd_buffer->surface_state_stream);
   anv_state_stream_init(&cmd_buffer->surface_state_stream,
                         &cmd_buffer->device->surface_state_pool, 4096);

   anv_state_stream_finish(&cmd_buffer->dynamic_state_stream);
   anv_state_stream_init(&cmd_buffer->dynamic_state_stream,
                         &cmd_buffer->device->dynamic_state_pool, 16384);
#endif
   return VK_SUCCESS;
}

void v3dvk_CmdBindTransformFeedbackBuffersEXT(
    VkCommandBuffer                             commandBuffer,
    uint32_t                                    firstBinding,
    uint32_t                                    bindingCount,
    const VkBuffer*                             pBuffers,
    const VkDeviceSize*                         pOffsets,
    const VkDeviceSize*                         pSizes)
{
   V3DVK_FROM_HANDLE(v3dvk_cmd_buffer, cmd_buffer, commandBuffer);
   struct v3dvk_xfb_binding *xfb = cmd_buffer->state.xfb_bindings;

   /* We have to defer setting up vertex buffer since we need the buffer
    * stride from the pipeline. */

   assert(firstBinding + bindingCount <= MAX_XFB_BUFFERS);
   for (uint32_t i = 0; i < bindingCount; i++) {
      if (pBuffers[i] == VK_NULL_HANDLE) {
         xfb[firstBinding + i].buffer = NULL;
      } else {
         V3DVK_FROM_HANDLE(v3dvk_buffer, buffer, pBuffers[i]);
         xfb[firstBinding + i].buffer = buffer;
         xfb[firstBinding + i].offset = pOffsets[i];
         xfb[firstBinding + i].size =
            v3dvk_buffer_get_range(buffer, pOffsets[i],
                                   pSizes ? pSizes[i] : VK_WHOLE_SIZE);
      }
   }
}
