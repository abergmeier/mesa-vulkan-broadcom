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

#include <assert.h>
#include <stdlib.h>
#define V3D_VERSION 42
#include "common/v3d_macros.h"
#include "v3d_cl.inl"
#include <cle/v3d_packet_v42_pack.h>
#include "util/macros.h"
#include "util/set.h"
#include "util/ralloc.h"
#include "util/hash_table.h"
#include "common.h"
#include "device.h"
#include "vk_alloc.h"
#include "v3dvk_buffer.h"
#include "v3dvk_cmd_buffer.h"
#include "v3dvk_cmd_pool.h"
#include "v3dvk_entrypoints.h"
#include "v3dvk_error.h"
#include "v3dvk_framebuffer.h"
#include "v3dvk_log.h"
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
v3dvk_cmd_buffer_add_bo(struct v3dvk_cmd_buffer *cmd, struct v3dvk_bo *bo)
{
   if (!bo)
      return;

   if (_mesa_set_search(cmd->bos, bo))
       return;
#if 0
   v3d_bo_reference(bo);
#endif
   _mesa_set_add(cmd->bos, bo);
#if 0
        job->referenced_size += bo->size;
#endif
   uint32_t *bo_handles = (void *)(uintptr_t)cmd->submit.bo_handles;

   if (cmd->submit.bo_handle_count >= cmd->bo_handles_size) {
      cmd->bo_handles_size = MAX2(4, cmd->bo_handles_size * 2);
      bo_handles = reralloc(cmd, bo_handles,
                            uint32_t, cmd->bo_handles_size);
      cmd->submit.bo_handles = (uintptr_t)(void *)bo_handles;
   }
   bo_handles[cmd->submit.bo_handle_count++] = bo->handle;
}

static VkResult
v3dvk_create_cmd_buffer(struct v3dvk_device *device,
                        struct v3dvk_cmd_pool *pool,
                        VkCommandBufferLevel level,
                        VkCommandBuffer *pCommandBuffer)
{
   struct v3dvk_cmd_buffer *cmd_buffer;
   cmd_buffer = vk_zalloc(&pool->alloc, sizeof(*cmd_buffer), 8,
                          VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
   if (cmd_buffer == NULL)
      return v3dvk_error(device->instance, VK_ERROR_OUT_OF_HOST_MEMORY);

   VkResult result = v3dvk_cmd_buffer_init(cmd_buffer, device, pool, level);
   if (result != VK_SUCCESS) {
      return result;
   }

   *pCommandBuffer = v3dvk_cmd_buffer_to_handle(cmd_buffer);
   return VK_SUCCESS;
}

VkResult
v3dvk_cmd_buffer_init(struct v3dvk_cmd_buffer *cmd_buffer,
                      struct v3dvk_device *device,
                      struct v3dvk_cmd_pool *pool,
                      VkCommandBufferLevel level)
{
   cmd_buffer->_loader_data.loaderMagic = ICD_LOADER_MAGIC;
   cmd_buffer->device = device;
   cmd_buffer->pool = pool;
   cmd_buffer->level = level;

   if (pool) {
      list_addtail(&cmd_buffer->pool_link, &pool->cmd_buffers);
      cmd_buffer->queue_family_index = pool->queue_family_index;

   } else {
      /* Init the pool_link so we can safely call list_del when we destroy
       * the command buffer
       */
      list_inithead(&cmd_buffer->pool_link);
      cmd_buffer->queue_family_index = V3DVK_QUEUE_GENERAL;
   }


   // _mesa_set_create uses ralloc details for ctx. Thus no sense to pass our
   // non-ralloced cmd_buffer
   cmd_buffer->bos = _mesa_set_create(NULL,
                                      _mesa_hash_pointer,
                                      _mesa_key_pointer_equal);
   v3d_init_cl(cmd_buffer, &cmd_buffer->bcl);
   v3d_init_cl(cmd_buffer, &cmd_buffer->rcl);
   v3d_init_cl(cmd_buffer, &cmd_buffer->indirect);

#if 0
   list_inithead(&cmd_buffer->upload.list);
   cmd_buffer->marker_reg = REG_A6XX_CP_SCRATCH_REG(
      cmd_buffer->level == VK_COMMAND_BUFFER_LEVEL_PRIMARY ? 7 : 6);
   VkResult result = tu_bo_init_new(device, &cmd_buffer->scratch_bo, 0x1000);
   if (result != VK_SUCCESS)
      return result;
#endif

   return VK_SUCCESS;
}


void
v3dvk_cmd_buffer_destroy(struct v3dvk_cmd_buffer *cmd_buffer)
{
#if 0
   tu_bo_finish(cmd_buffer->device, &cmd_buffer->scratch_bo);
#endif
   list_del(&cmd_buffer->pool_link);
#if 0
   for (unsigned i = 0; i < VK_PIPELINE_BIND_POINT_RANGE_SIZE; i++)
      free(cmd_buffer->descriptors[i].push_set.set.mapped_ptr);
#endif

   v3d_destroy_cl(&cmd_buffer->bcl);
   v3d_destroy_cl(&cmd_buffer->rcl);
   v3d_destroy_cl(&cmd_buffer->indirect);

   set_foreach(cmd_buffer->bos, entry) {
      struct v3d_bo *bo = (struct v3d_bo *)entry->key;
#if 0
      v3d_bo_unreference(&bo);
#endif
   }
#if 0
   vk_free(&cmd_buffer->pool->alloc, cmd_buffer);
#endif
}

VkResult
v3dvk_cmd_buffer_reset(struct v3dvk_cmd_buffer *cmd_buffer)
{

   v3dvk_cmd_buffer_destroy(cmd_buffer);
   return v3dvk_cmd_buffer_init(cmd_buffer, cmd_buffer->device, cmd_buffer->pool, cmd_buffer->level);
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
v3dvk_AllocateCommandBuffers(VkDevice _device,
                             const VkCommandBufferAllocateInfo *pAllocateInfo,
                             VkCommandBuffer *pCommandBuffers)
{
   V3DVK_FROM_HANDLE(v3dvk_device, device, _device);
   V3DVK_FROM_HANDLE(v3dvk_cmd_pool, pool, pAllocateInfo->commandPool);

   VkResult result = VK_SUCCESS;
   uint32_t i;

   for (i = 0; i < pAllocateInfo->commandBufferCount; i++) {

      if (!list_is_empty(&pool->free_cmd_buffers)) {
         struct v3dvk_cmd_buffer *cmd_buffer = list_first_entry(
            &pool->free_cmd_buffers, struct v3dvk_cmd_buffer, pool_link);

         list_del(&cmd_buffer->pool_link);
         list_addtail(&cmd_buffer->pool_link, &pool->cmd_buffers);

         result = v3dvk_cmd_buffer_reset(cmd_buffer);
         cmd_buffer->_loader_data.loaderMagic = ICD_LOADER_MAGIC;
         cmd_buffer->level = pAllocateInfo->level;

         pCommandBuffers[i] = v3dvk_cmd_buffer_to_handle(cmd_buffer);
      } else {
         result = v3dvk_create_cmd_buffer(device, pool, pAllocateInfo->level,
                                       &pCommandBuffers[i]);
      }
      if (result != VK_SUCCESS)
         break;
   }

   if (result != VK_SUCCESS) {
      v3dvk_FreeCommandBuffers(_device, pAllocateInfo->commandPool, i,
                               pCommandBuffers);

      /* From the Vulkan 1.0.66 spec:
       *
       * "vkAllocateCommandBuffers can be used to create multiple
       *  command buffers. If the creation of any of those command
       *  buffers fails, the implementation must destroy all
       *  successfully created command buffer objects from this
       *  command, set all entries of the pCommandBuffers array to
       *  NULL and return the error."
       */
      memset(pCommandBuffers, 0,
             sizeof(*pCommandBuffers) * pAllocateInfo->commandBufferCount);
   }

   return result;
}

void
v3dvk_FreeCommandBuffers(VkDevice device,
                         VkCommandPool commandPool,
                         uint32_t commandBufferCount,
                         const VkCommandBuffer *pCommandBuffers)
{
   for (uint32_t i = 0; i < commandBufferCount; i++) {
      V3DVK_FROM_HANDLE(v3dvk_cmd_buffer, cmd_buffer, pCommandBuffers[i]);

      if (cmd_buffer) {
         if (cmd_buffer->pool) {
            list_del(&cmd_buffer->pool_link);
            list_addtail(&cmd_buffer->pool_link,
                         &cmd_buffer->pool->free_cmd_buffers);
         } else
            v3dvk_cmd_buffer_destroy(cmd_buffer);
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

void
v3dvk_CmdBeginRenderPass(VkCommandBuffer commandBuffer,
                         const VkRenderPassBeginInfo *pRenderPassBegin,
                         VkSubpassContents contents)
{
   V3DVK_FROM_HANDLE(v3dvk_cmd_buffer, cmd_buffer, commandBuffer);
   V3DVK_FROM_HANDLE(v3dvk_render_pass, pass, pRenderPassBegin->renderPass);
   V3DVK_FROM_HANDLE(v3dvk_framebuffer, framebuffer, pRenderPassBegin->framebuffer);
   VkResult result;

   cmd_buffer->state.pass = pass;
   cmd_buffer->state.subpass = pass->subpasses;
   cmd_buffer->state.framebuffer = framebuffer;
#if 0
   result = tu_cmd_state_setup_attachments(cmd_buffer, pRenderPassBegin);
   if (result != VK_SUCCESS)
      return;

   tu_cmd_update_tiling_config(cmd_buffer, &pRenderPassBegin->renderArea);
   tu_cmd_prepare_tile_load_ib(cmd_buffer);
   tu_cmd_prepare_tile_store_ib(cmd_buffer);
#endif
   /* Get space to emit our BCL state, using a branch to jump to a new BO
    * if necessary.
    */
   v3d_cl_ensure_space_with_branch(&cmd_buffer->bcl, 256 /* XXX */);

   cl_emit(&cmd_buffer->bcl, TILE_BINNING_MODE_CFG, config) {
      // FIXME: No idea how to implement offset natively in V3D
      config.width_in_pixels = pRenderPassBegin->renderArea.offset.x + pRenderPassBegin->renderArea.extent.width;
      config.height_in_pixels = pRenderPassBegin->renderArea.offset.y + pRenderPassBegin->renderArea.extent.height;
      config.number_of_render_targets =
         1;
#if 0
         MAX2(cmd_buffer->state.framebuffer->nr_cbufs, 1);
      config.multisample_mode_4x = job->msaa;
      config.maximum_bpp_of_all_render_targets = job->internal_bpp;
#endif
   }

   /* There's definitely nothing in the VCD cache we want. */
   cl_emit(&cmd_buffer->bcl, FLUSH_VCD_CACHE, bin);

   /* Disable any leftover OQ state from another job. */
   cl_emit(&cmd_buffer->bcl, OCCLUSION_QUERY_COUNTER, counter);

   /* "Binning mode lists must have a Start Tile Binning item (6) after
    *  any prefix state data before the binning list proper starts."
    */
   cl_emit(&cmd_buffer->bcl, START_TILE_BINNING, bin);
#if 0
   job->draw_width = v3d->framebuffer.width;
   job->draw_height = v3d->framebuffer.height;
#endif
}

void
v3dvk_CmdBindPipeline(VkCommandBuffer commandBuffer,
                      VkPipelineBindPoint pipelineBindPoint,
                      VkPipeline _pipeline)
{
   V3DVK_FROM_HANDLE(v3dvk_cmd_buffer, cmd, commandBuffer);
   V3DVK_FROM_HANDLE(v3dvk_pipeline, pipeline, _pipeline);

   switch (pipelineBindPoint) {
   case VK_PIPELINE_BIND_POINT_GRAPHICS:
      cmd->state.pipeline = pipeline;
      break;
   case VK_PIPELINE_BIND_POINT_COMPUTE:
      V3DVK_FINISHME("binding compute pipeline");
      break;
   default:
      unreachable("unrecognized pipeline bind point");
      break;
   }
}

void
v3dvk_CmdEndRenderPass(VkCommandBuffer commandBuffer)
{
   V3DVK_FROM_HANDLE(v3dvk_cmd_buffer, cmd_buffer, commandBuffer);

#if 0
   v3dvk_cmd_render_tiles(cmd_buffer);

   /* discard draw_cs entries now that the tiles are rendered */
   v3dvk_cs_discard_entries(&cmd_buffer->draw_cs);

#endif
   vk_free(&cmd_buffer->pool->alloc, cmd_buffer->state.attachments);
   cmd_buffer->state.attachments = NULL;

   cmd_buffer->state.pass = NULL;
   cmd_buffer->state.subpass = NULL;
   cmd_buffer->state.framebuffer = NULL;
}
