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

VkResult
v3dvk_cmd_buffer_execbuf(struct v3dvk_device *device,
                       struct v3dvk_cmd_buffer *cmd_buffer,
                       const VkSemaphore *in_semaphores,
                       uint32_t num_in_semaphores,
                       const VkSemaphore *out_semaphores,
                       uint32_t num_out_semaphores,
                       VkFence _fence)
{
   V3DVK_FROM_HANDLE(v3dvk_fence, fence, _fence);
#if 0
   UNUSED struct anv_physical_device *pdevice = &device->instance->physicalDevice;

   struct anv_execbuf execbuf;
   anv_execbuf_init(&execbuf);

   int in_fence = -1;
#endif
   VkResult result = VK_SUCCESS;
#if 0
   for (uint32_t i = 0; i < num_in_semaphores; i++) {
      ANV_FROM_HANDLE(anv_semaphore, semaphore, in_semaphores[i]);
      struct anv_semaphore_impl *impl =
         semaphore->temporary.type != ANV_SEMAPHORE_TYPE_NONE ?
         &semaphore->temporary : &semaphore->permanent;

      switch (impl->type) {
      case ANV_SEMAPHORE_TYPE_BO:
         assert(!pdevice->has_syncobj);
         result = anv_execbuf_add_bo(&execbuf, impl->bo, NULL,
                                     0, &device->alloc);
         if (result != VK_SUCCESS)
            return result;
         break;

      case ANV_SEMAPHORE_TYPE_SYNC_FILE:
         assert(!pdevice->has_syncobj);
         if (in_fence == -1) {
            in_fence = impl->fd;
         } else {
            int merge = anv_gem_sync_file_merge(device, in_fence, impl->fd);
            if (merge == -1)
               return vk_error(VK_ERROR_INVALID_EXTERNAL_HANDLE);

            close(impl->fd);
            close(in_fence);
            in_fence = merge;
         }

         impl->fd = -1;
         break;

      case ANV_SEMAPHORE_TYPE_DRM_SYNCOBJ:
         result = anv_execbuf_add_syncobj(&execbuf, impl->syncobj,
                                          I915_EXEC_FENCE_WAIT,
                                          &device->alloc);
         if (result != VK_SUCCESS)
            return result;
         break;

      default:
         break;
      }
   }

   bool need_out_fence = false;
   for (uint32_t i = 0; i < num_out_semaphores; i++) {
      ANV_FROM_HANDLE(anv_semaphore, semaphore, out_semaphores[i]);

      /* Under most circumstances, out fences won't be temporary.  However,
       * the spec does allow it for opaque_fd.  From the Vulkan 1.0.53 spec:
       *
       *    "If the import is temporary, the implementation must restore the
       *    semaphore to its prior permanent state after submitting the next
       *    semaphore wait operation."
       *
       * The spec says nothing whatsoever about signal operations on
       * temporarily imported semaphores so it appears they are allowed.
       * There are also CTS tests that require this to work.
       */
      struct anv_semaphore_impl *impl =
         semaphore->temporary.type != ANV_SEMAPHORE_TYPE_NONE ?
         &semaphore->temporary : &semaphore->permanent;

      switch (impl->type) {
      case ANV_SEMAPHORE_TYPE_BO:
         assert(!pdevice->has_syncobj);
         result = anv_execbuf_add_bo(&execbuf, impl->bo, NULL,
                                     EXEC_OBJECT_WRITE, &device->alloc);
         if (result != VK_SUCCESS)
            return result;
         break;

      case ANV_SEMAPHORE_TYPE_SYNC_FILE:
         assert(!pdevice->has_syncobj);
         need_out_fence = true;
         break;

      case ANV_SEMAPHORE_TYPE_DRM_SYNCOBJ:
         result = anv_execbuf_add_syncobj(&execbuf, impl->syncobj,
                                          I915_EXEC_FENCE_SIGNAL,
                                          &device->alloc);
         if (result != VK_SUCCESS)
            return result;
         break;

      default:
         break;
      }
   }

   if (fence) {
      /* Under most circumstances, out fences won't be temporary.  However,
       * the spec does allow it for opaque_fd.  From the Vulkan 1.0.53 spec:
       *
       *    "If the import is temporary, the implementation must restore the
       *    semaphore to its prior permanent state after submitting the next
       *    semaphore wait operation."
       *
       * The spec says nothing whatsoever about signal operations on
       * temporarily imported semaphores so it appears they are allowed.
       * There are also CTS tests that require this to work.
       */
      struct anv_fence_impl *impl =
         fence->temporary.type != ANV_FENCE_TYPE_NONE ?
         &fence->temporary : &fence->permanent;

      switch (impl->type) {
      case ANV_FENCE_TYPE_BO:
         assert(!pdevice->has_syncobj_wait);
         result = anv_execbuf_add_bo(&execbuf, &impl->bo.bo, NULL,
                                     EXEC_OBJECT_WRITE, &device->alloc);
         if (result != VK_SUCCESS)
            return result;
         break;

      case ANV_FENCE_TYPE_SYNCOBJ:
         result = anv_execbuf_add_syncobj(&execbuf, impl->syncobj,
                                          I915_EXEC_FENCE_SIGNAL,
                                          &device->alloc);
         if (result != VK_SUCCESS)
            return result;
         break;

      default:
         unreachable("Invalid fence type");
      }
   }

   if (cmd_buffer) {
      if (unlikely(INTEL_DEBUG & DEBUG_BATCH)) {
         struct anv_batch_bo **bo = u_vector_head(&cmd_buffer->seen_bbos);

         device->cmd_buffer_being_decoded = cmd_buffer;
         gen_print_batch(&device->decoder_ctx, (*bo)->bo.map,
                         (*bo)->bo.size, (*bo)->bo.offset, false);
         device->cmd_buffer_being_decoded = NULL;
      }

      result = setup_execbuf_for_cmd_buffer(&execbuf, cmd_buffer);
   } else {
      result = setup_empty_execbuf(&execbuf, device);
   }

   if (result != VK_SUCCESS)
      return result;

   if (execbuf.fence_count > 0) {
      assert(device->instance->physicalDevice.has_syncobj);
      execbuf.execbuf.flags |= I915_EXEC_FENCE_ARRAY;
      execbuf.execbuf.num_cliprects = execbuf.fence_count;
      execbuf.execbuf.cliprects_ptr = (uintptr_t) execbuf.fences;
   }

   if (in_fence != -1) {
      execbuf.execbuf.flags |= I915_EXEC_FENCE_IN;
      execbuf.execbuf.rsvd2 |= (uint32_t)in_fence;
   }

   if (need_out_fence)
      execbuf.execbuf.flags |= I915_EXEC_FENCE_OUT;

   result = anv_device_execbuf(device, &execbuf.execbuf, execbuf.bos);

   /* Execbuf does not consume the in_fence.  It's our job to close it. */
   if (in_fence != -1)
      close(in_fence);

   for (uint32_t i = 0; i < num_in_semaphores; i++) {
      ANV_FROM_HANDLE(anv_semaphore, semaphore, in_semaphores[i]);
      /* From the Vulkan 1.0.53 spec:
       *
       *    "If the import is temporary, the implementation must restore the
       *    semaphore to its prior permanent state after submitting the next
       *    semaphore wait operation."
       *
       * This has to happen after the execbuf in case we close any syncobjs in
       * the process.
       */
      anv_semaphore_reset_temporary(device, semaphore);
   }

   if (fence && fence->permanent.type == ANV_FENCE_TYPE_BO) {
      assert(!pdevice->has_syncobj_wait);
      /* BO fences can't be shared, so they can't be temporary. */
      assert(fence->temporary.type == ANV_FENCE_TYPE_NONE);

      /* Once the execbuf has returned, we need to set the fence state to
       * SUBMITTED.  We can't do this before calling execbuf because
       * anv_GetFenceStatus does take the global device lock before checking
       * fence->state.
       *
       * We set the fence state to SUBMITTED regardless of whether or not the
       * execbuf succeeds because we need to ensure that vkWaitForFences() and
       * vkGetFenceStatus() return a valid result (VK_ERROR_DEVICE_LOST or
       * VK_SUCCESS) in a finite amount of time even if execbuf fails.
       */
      fence->permanent.bo.state = ANV_BO_FENCE_STATE_SUBMITTED;
   }

   if (result == VK_SUCCESS && need_out_fence) {
      assert(!pdevice->has_syncobj_wait);
      int out_fence = execbuf.execbuf.rsvd2 >> 32;
      for (uint32_t i = 0; i < num_out_semaphores; i++) {
         ANV_FROM_HANDLE(anv_semaphore, semaphore, out_semaphores[i]);
         /* Out fences can't have temporary state because that would imply
          * that we imported a sync file and are trying to signal it.
          */
         assert(semaphore->temporary.type == ANV_SEMAPHORE_TYPE_NONE);
         struct anv_semaphore_impl *impl = &semaphore->permanent;

         if (impl->type == ANV_SEMAPHORE_TYPE_SYNC_FILE) {
            assert(impl->fd == -1);
            impl->fd = dup(out_fence);
         }
      }
      close(out_fence);
   }

   anv_execbuf_finish(&execbuf, &device->alloc);
#endif
   return result;
}
