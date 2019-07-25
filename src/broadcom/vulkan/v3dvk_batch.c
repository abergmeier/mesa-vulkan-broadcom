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

#include <string.h>
#include "vulkan/util/vk_alloc.h"
#include "common.h"
#include "device.h"
#include "instance.h"
#include "v3dvk_cmd_buffer.h"
#include "v3dvk_batch.h"
#include "v3dvk_fence.h"
#include "v3dvk_semaphore.h"

struct v3dvk_execbuf {
#if 0
   struct drm_i915_gem_execbuffer2           execbuf;

   struct drm_i915_gem_exec_object2 *        objects;
   uint32_t                                  bo_count;
   struct anv_bo **                          bos;

   /* Allocated length of the 'objects' and 'bos' arrays */
   uint32_t                                  array_length;

   bool                                      has_relocs;

   uint32_t                                  fence_count;
   uint32_t                                  fence_array_length;
   struct drm_i915_gem_exec_fence *          fences;
   struct anv_syncobj **                     syncobjs;
#endif
};

static void
v3dvk_execbuf_init(struct v3dvk_execbuf *exec)
{
   memset(exec, 0, sizeof(*exec));
}

static void
v3dvk_execbuf_finish(struct v3dvk_execbuf *exec,
                     const VkAllocationCallbacks *alloc)
{
#if 0
   vk_free(alloc, exec->objects);
   vk_free(alloc, exec->bos);
   vk_free(alloc, exec->fences);
   vk_free(alloc, exec->syncobjs);
#endif
}

static VkResult
setup_execbuf_for_cmd_buffer(struct v3dvk_execbuf *execbuf,
                             struct v3dvk_cmd_buffer *cmd_buffer)
{
   struct v3dvk_batch *batch = &cmd_buffer->batch;
#if 0
   struct anv_state_pool *ss_pool =
      &cmd_buffer->device->surface_state_pool;

   adjust_relocations_from_state_pool(ss_pool, &cmd_buffer->surface_relocs,
                                      cmd_buffer->last_ss_pool_center);
   VkResult result;
   struct anv_bo *bo;
   if (cmd_buffer->device->instance->physicalDevice.use_softpin) {
      anv_block_pool_foreach_bo(bo, &ss_pool->block_pool) {
         result = anv_execbuf_add_bo(execbuf, bo, NULL, 0,
                                     &cmd_buffer->device->alloc);
         if (result != VK_SUCCESS)
            return result;
      }
      /* Add surface dependencies (BOs) to the execbuf */
      anv_execbuf_add_bo_set(execbuf, cmd_buffer->surface_relocs.deps, 0,
                             &cmd_buffer->device->alloc);

      /* Add the BOs for all memory objects */
      list_for_each_entry(struct anv_device_memory, mem,
                          &cmd_buffer->device->memory_objects, link) {
         result = anv_execbuf_add_bo(execbuf, mem->bo, NULL, 0,
                                     &cmd_buffer->device->alloc);
         if (result != VK_SUCCESS)
            return result;
      }

      struct anv_block_pool *pool;
      pool = &cmd_buffer->device->dynamic_state_pool.block_pool;
      anv_block_pool_foreach_bo(bo, pool) {
         result = anv_execbuf_add_bo(execbuf, bo, NULL, 0,
                                     &cmd_buffer->device->alloc);
         if (result != VK_SUCCESS)
            return result;
      }

      pool = &cmd_buffer->device->instruction_state_pool.block_pool;
      anv_block_pool_foreach_bo(bo, pool) {
         result = anv_execbuf_add_bo(execbuf, bo, NULL, 0,
                                     &cmd_buffer->device->alloc);
         if (result != VK_SUCCESS)
            return result;
      }

      pool = &cmd_buffer->device->binding_table_pool.block_pool;
      anv_block_pool_foreach_bo(bo, pool) {
         result = anv_execbuf_add_bo(execbuf, bo, NULL, 0,
                                     &cmd_buffer->device->alloc);
         if (result != VK_SUCCESS)
            return result;
      }
   } else {
      /* Since we aren't in the softpin case, all of our STATE_BASE_ADDRESS BOs
       * will get added automatically by processing relocations on the batch
       * buffer.  We have to add the surface state BO manually because it has
       * relocations of its own that we need to be sure are processsed.
       */
      result = anv_execbuf_add_bo(execbuf, ss_pool->block_pool.bo,
                                  &cmd_buffer->surface_relocs, 0,
                                  &cmd_buffer->device->alloc);
      if (result != VK_SUCCESS)
         return result;
   }

   /* First, we walk over all of the bos we've seen and add them and their
    * relocations to the validate list.
    */
   struct anv_batch_bo **bbo;
   u_vector_foreach(bbo, &cmd_buffer->seen_bbos) {
      adjust_relocations_to_state_pool(ss_pool, &(*bbo)->bo, &(*bbo)->relocs,
                                       cmd_buffer->last_ss_pool_center);

      result = anv_execbuf_add_bo(execbuf, &(*bbo)->bo, &(*bbo)->relocs, 0,
                                  &cmd_buffer->device->alloc);
      if (result != VK_SUCCESS)
         return result;
   }

   /* Now that we've adjusted all of the surface state relocations, we need to
    * record the surface state pool center so future executions of the command
    * buffer can adjust correctly.
    */
   cmd_buffer->last_ss_pool_center = ss_pool->block_pool.center_bo_offset;

   struct anv_batch_bo *first_batch_bo =
      list_first_entry(&cmd_buffer->batch_bos, struct anv_batch_bo, link);

   /* The kernel requires that the last entry in the validation list be the
    * batch buffer to execute.  We can simply swap the element
    * corresponding to the first batch_bo in the chain with the last
    * element in the list.
    */
   if (first_batch_bo->bo.index != execbuf->bo_count - 1) {
      uint32_t idx = first_batch_bo->bo.index;
      uint32_t last_idx = execbuf->bo_count - 1;

      struct drm_i915_gem_exec_object2 tmp_obj = execbuf->objects[idx];
      assert(execbuf->bos[idx] == &first_batch_bo->bo);

      execbuf->objects[idx] = execbuf->objects[last_idx];
      execbuf->bos[idx] = execbuf->bos[last_idx];
      execbuf->bos[idx]->index = idx;

      execbuf->objects[last_idx] = tmp_obj;
      execbuf->bos[last_idx] = &first_batch_bo->bo;
      first_batch_bo->bo.index = last_idx;
   }

   /* If we are pinning our BOs, we shouldn't have to relocate anything */
   if (cmd_buffer->device->instance->physicalDevice.use_softpin)
      assert(!execbuf->has_relocs);

   /* Now we go through and fixup all of the relocation lists to point to
    * the correct indices in the object array.  We have to do this after we
    * reorder the list above as some of the indices may have changed.
    */
   if (execbuf->has_relocs) {
      u_vector_foreach(bbo, &cmd_buffer->seen_bbos)
         anv_cmd_buffer_process_relocs(cmd_buffer, &(*bbo)->relocs);

      anv_cmd_buffer_process_relocs(cmd_buffer, &cmd_buffer->surface_relocs);
   }

   if (!cmd_buffer->device->info.has_llc) {
      __builtin_ia32_mfence();
      u_vector_foreach(bbo, &cmd_buffer->seen_bbos) {
         for (uint32_t i = 0; i < (*bbo)->length; i += CACHELINE_SIZE)
            __builtin_ia32_clflush((*bbo)->bo.map + i);
      }
   }

   execbuf->execbuf = (struct drm_i915_gem_execbuffer2) {
      .buffers_ptr = (uintptr_t) execbuf->objects,
      .buffer_count = execbuf->bo_count,
      .batch_start_offset = 0,
      .batch_len = batch->next - batch->start,
      .cliprects_ptr = 0,
      .num_cliprects = 0,
      .DR1 = 0,
      .DR4 = 0,
      .flags = I915_EXEC_HANDLE_LUT | I915_EXEC_RENDER,
      .rsvd1 = cmd_buffer->device->context_id,
      .rsvd2 = 0,
   };

   if (relocate_cmd_buffer(cmd_buffer, execbuf)) {
      /* If we were able to successfully relocate everything, tell the kernel
       * that it can skip doing relocations. The requirement for using
       * NO_RELOC is:
       *
       *  1) The addresses written in the objects must match the corresponding
       *     reloc.presumed_offset which in turn must match the corresponding
       *     execobject.offset.
       *
       *  2) To avoid stalling, execobject.offset should match the current
       *     address of that object within the active context.
       *
       * In order to satisfy all of the invariants that make userspace
       * relocations to be safe (see relocate_cmd_buffer()), we need to
       * further ensure that the addresses we use match those used by the
       * kernel for the most recent execbuf2.
       *
       * The kernel may still choose to do relocations anyway if something has
       * moved in the GTT. In this case, the relocation list still needs to be
       * valid.  All relocations on the batch buffers are already valid and
       * kept up-to-date.  For surface state relocations, by applying the
       * relocations in relocate_cmd_buffer, we ensured that the address in
       * the RENDER_SURFACE_STATE matches presumed_offset, so it should be
       * safe for the kernel to relocate them as needed.
       */
      execbuf->execbuf.flags |= I915_EXEC_NO_RELOC;
   } else {
      /* In the case where we fall back to doing kernel relocations, we need
       * to ensure that the relocation list is valid.  All relocations on the
       * batch buffers are already valid and kept up-to-date.  Since surface
       * states are shared between command buffers and we don't know what
       * order they will be submitted to the kernel, we don't know what
       * address is actually written in the surface state object at any given
       * time.  The only option is to set a bogus presumed offset and let the
       * kernel relocate them.
       */
      for (size_t i = 0; i < cmd_buffer->surface_relocs.num_relocs; i++)
         cmd_buffer->surface_relocs.relocs[i].presumed_offset = -1;
   }
#endif
   return VK_SUCCESS;
}

static VkResult
setup_empty_execbuf(struct v3dvk_execbuf *execbuf, struct v3dvk_device *device)
{
#if 0
   VkResult result = anv_execbuf_add_bo(execbuf, &device->trivial_batch_bo,
                                        NULL, 0, &device->alloc);
   if (result != VK_SUCCESS)
      return result;

   execbuf->execbuf = (struct drm_i915_gem_execbuffer2) {
      .buffers_ptr = (uintptr_t) execbuf->objects,
      .buffer_count = execbuf->bo_count,
      .batch_start_offset = 0,
      .batch_len = 8, /* GEN7_MI_BATCH_BUFFER_END and NOOP */
      .flags = I915_EXEC_HANDLE_LUT | I915_EXEC_RENDER,
      .rsvd1 = device->context_id,
      .rsvd2 = 0,
   };
#endif
   return VK_SUCCESS;
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
   struct v3dvk_physical_device *pdevice = &device->instance->physicalDevice;

   struct v3dvk_execbuf execbuf;
   v3dvk_execbuf_init(&execbuf);

   int in_fence = -1;
   VkResult result = VK_SUCCESS;
   for (uint32_t i = 0; i < num_in_semaphores; i++) {
      V3DVK_FROM_HANDLE(v3dvk_semaphore, semaphore, in_semaphores[i]);
      struct v3dvk_semaphore_impl *impl =
         semaphore->temporary.type != V3DVK_SEMAPHORE_TYPE_NONE ?
         &semaphore->temporary : &semaphore->permanent;

      switch (impl->type) {
#if 0
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
#endif
      default:
         break;
      }
   }

   bool need_out_fence = false;
   for (uint32_t i = 0; i < num_out_semaphores; i++) {
      V3DVK_FROM_HANDLE(v3dvk_semaphore, semaphore, out_semaphores[i]);

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
      struct v3dvk_semaphore_impl *impl =
         semaphore->temporary.type != V3DVK_SEMAPHORE_TYPE_NONE ?
         &semaphore->temporary : &semaphore->permanent;

      switch (impl->type) {
#if 0
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
#endif
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
      struct v3dvk_fence_impl *impl =
         fence->temporary.type != V3DVK_FENCE_TYPE_NONE ?
         &fence->temporary : &fence->permanent;

      switch (impl->type) {
#if 0
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
#endif
      default:
         unreachable("Invalid fence type");
      }
   }

   if (cmd_buffer) {
#if 0
      if (unlikely(INTEL_DEBUG & DEBUG_BATCH)) {
         struct anv_batch_bo **bo = u_vector_head(&cmd_buffer->seen_bbos);

         device->cmd_buffer_being_decoded = cmd_buffer;
         gen_print_batch(&device->decoder_ctx, (*bo)->bo.map,
                         (*bo)->bo.size, (*bo)->bo.offset, false);
         device->cmd_buffer_being_decoded = NULL;
      }
#endif
      result = setup_execbuf_for_cmd_buffer(&execbuf, cmd_buffer);
   } else {
      result = setup_empty_execbuf(&execbuf, device);
   }

   if (result != VK_SUCCESS)
      return result;
#if 0
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
#endif
   v3dvk_execbuf_finish(&execbuf, &device->alloc);

   return result;
}
