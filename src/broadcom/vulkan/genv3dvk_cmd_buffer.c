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
#include <vulkan/vulkan.h>
// Needs to be in front to define certain symbols
#include "v3d_cl.h"
#define V3D_VERSION 42
#include "cle/v3dx_pack.h"
#include "common.h"
#include "v3dvk_cmd_buffer.h"
#include "genv3dvk_cmd_buffer.h"
#include "v3dvk_entrypoints.h"

#define GENX(X) V3D42_##X
#define genX(x) V3D42_##x

/**
 * Setup anv_cmd_state::attachments for vkCmdBeginRenderPass.
 */
static VkResult
v3dvk_cmd_buffer_setup_attachments(struct v3dvk_cmd_buffer *cmd_buffer,
                                   struct v3dvk_render_pass *pass,
                                   const VkRenderPassBeginInfo *begin)
{
#if 0
   const struct isl_device *isl_dev = &cmd_buffer->device->isl_dev;
   struct anv_cmd_state *state = &cmd_buffer->state;

   vk_free(&cmd_buffer->pool->alloc, state->attachments);

   if (pass->attachment_count > 0) {
      state->attachments = vk_alloc(&cmd_buffer->pool->alloc,
                                    pass->attachment_count *
                                         sizeof(state->attachments[0]),
                                    8, VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
      if (state->attachments == NULL) {
         /* Propagate VK_ERROR_OUT_OF_HOST_MEMORY to vkEndCommandBuffer */
         return anv_batch_set_error(&cmd_buffer->batch,
                                    VK_ERROR_OUT_OF_HOST_MEMORY);
      }
   } else {
      state->attachments = NULL;
   }

   /* Reserve one for the NULL state. */
   unsigned num_states = 1;
   for (uint32_t i = 0; i < pass->attachment_count; ++i) {
      if (vk_format_is_color(pass->attachments[i].format))
         num_states++;

      if (need_input_attachment_state(&pass->attachments[i]))
         num_states++;
   }

   const uint32_t ss_stride = align_u32(isl_dev->ss.size, isl_dev->ss.align);
   state->render_pass_states =
      anv_state_stream_alloc(&cmd_buffer->surface_state_stream,
                             num_states * ss_stride, isl_dev->ss.align);

   struct anv_state next_state = state->render_pass_states;
   next_state.alloc_size = isl_dev->ss.size;

   state->null_surface_state = next_state;
   next_state.offset += ss_stride;
   next_state.map += ss_stride;

   for (uint32_t i = 0; i < pass->attachment_count; ++i) {
      if (vk_format_is_color(pass->attachments[i].format)) {
         state->attachments[i].color.state = next_state;
         next_state.offset += ss_stride;
         next_state.map += ss_stride;
      }

      if (need_input_attachment_state(&pass->attachments[i])) {
         state->attachments[i].input.state = next_state;
         next_state.offset += ss_stride;
         next_state.map += ss_stride;
      }
   }
   assert(next_state.offset == state->render_pass_states.offset +
                               state->render_pass_states.alloc_size);

   if (begin) {
      ANV_FROM_HANDLE(anv_framebuffer, framebuffer, begin->framebuffer);
      assert(pass->attachment_count == framebuffer->attachment_count);

      isl_null_fill_state(isl_dev, state->null_surface_state.map,
                          isl_extent3d(framebuffer->width,
                                       framebuffer->height,
                                       framebuffer->layers));

      for (uint32_t i = 0; i < pass->attachment_count; ++i) {
         struct anv_render_pass_attachment *att = &pass->attachments[i];
         VkImageAspectFlags att_aspects = vk_format_aspects(att->format);
         VkImageAspectFlags clear_aspects = 0;
         VkImageAspectFlags load_aspects = 0;

         if (att_aspects & VK_IMAGE_ASPECT_ANY_COLOR_BIT_ANV) {
            /* color attachment */
            if (att->load_op == VK_ATTACHMENT_LOAD_OP_CLEAR) {
               clear_aspects |= VK_IMAGE_ASPECT_COLOR_BIT;
            } else if (att->load_op == VK_ATTACHMENT_LOAD_OP_LOAD) {
               load_aspects |= VK_IMAGE_ASPECT_COLOR_BIT;
            }
         } else {
            /* depthstencil attachment */
            if (att_aspects & VK_IMAGE_ASPECT_DEPTH_BIT) {
               if (att->load_op == VK_ATTACHMENT_LOAD_OP_CLEAR) {
                  clear_aspects |= VK_IMAGE_ASPECT_DEPTH_BIT;
               } else if (att->load_op == VK_ATTACHMENT_LOAD_OP_LOAD) {
                  load_aspects |= VK_IMAGE_ASPECT_DEPTH_BIT;
               }
            }
            if (att_aspects & VK_IMAGE_ASPECT_STENCIL_BIT) {
               if (att->stencil_load_op == VK_ATTACHMENT_LOAD_OP_CLEAR) {
                  clear_aspects |= VK_IMAGE_ASPECT_STENCIL_BIT;
               } else if (att->stencil_load_op == VK_ATTACHMENT_LOAD_OP_LOAD) {
                  load_aspects |= VK_IMAGE_ASPECT_STENCIL_BIT;
               }
            }
         }

         state->attachments[i].current_layout = att->initial_layout;
         state->attachments[i].pending_clear_aspects = clear_aspects;
         state->attachments[i].pending_load_aspects = load_aspects;
         if (clear_aspects)
            state->attachments[i].clear_value = begin->pClearValues[i];

         struct anv_image_view *iview = framebuffer->attachments[i];
         anv_assert(iview->vk_format == att->format);

         const uint32_t num_layers = iview->planes[0].isl.array_len;
         state->attachments[i].pending_clear_views = (1 << num_layers) - 1;

         union isl_color_value clear_color = { .u32 = { 0, } };
         if (att_aspects & VK_IMAGE_ASPECT_ANY_COLOR_BIT_ANV) {
            anv_assert(iview->n_planes == 1);
            assert(att_aspects == VK_IMAGE_ASPECT_COLOR_BIT);
            color_attachment_compute_aux_usage(cmd_buffer->device,
                                               state, i, begin->renderArea,
                                               &clear_color);

            anv_image_fill_surface_state(cmd_buffer->device,
                                         iview->image,
                                         VK_IMAGE_ASPECT_COLOR_BIT,
                                         &iview->planes[0].isl,
                                         ISL_SURF_USAGE_RENDER_TARGET_BIT,
                                         state->attachments[i].aux_usage,
                                         &clear_color,
                                         0,
                                         &state->attachments[i].color,
                                         NULL);

            add_surface_state_relocs(cmd_buffer, state->attachments[i].color);
         } else {
            depth_stencil_attachment_compute_aux_usage(cmd_buffer->device,
                                                       state, i,
                                                       begin->renderArea);
         }

         if (need_input_attachment_state(&pass->attachments[i])) {
            anv_image_fill_surface_state(cmd_buffer->device,
                                         iview->image,
                                         VK_IMAGE_ASPECT_COLOR_BIT,
                                         &iview->planes[0].isl,
                                         ISL_SURF_USAGE_TEXTURE_BIT,
                                         state->attachments[i].input_aux_usage,
                                         &clear_color,
                                         0,
                                         &state->attachments[i].input,
                                         NULL);

            add_surface_state_relocs(cmd_buffer, state->attachments[i].input);
         }
      }
   }
#endif
   return VK_SUCCESS;
}

VkResult
v3dvk_BeginCommandBuffer(
    VkCommandBuffer                             commandBuffer,
    const VkCommandBufferBeginInfo*             pBeginInfo)
{
   V3DVK_FROM_HANDLE(v3dvk_cmd_buffer, cmd_buffer, commandBuffer);

   /* If this is the first vkBeginCommandBuffer, we must *initialize* the
    * command buffer's state. Otherwise, we must *reset* its state. In both
    * cases we reset it.
    *
    * From the Vulkan 1.0 spec:
    *
    *    If a command buffer is in the executable state and the command buffer
    *    was allocated from a command pool with the
    *    VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT flag set, then
    *    vkBeginCommandBuffer implicitly resets the command buffer, behaving
    *    as if vkResetCommandBuffer had been called with
    *    VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT not set. It then puts
    *    the command buffer in the recording state.
    */
   v3dvk_cmd_buffer_reset(cmd_buffer);

   cmd_buffer->usage_flags = pBeginInfo->flags;

   assert(cmd_buffer->level == VK_COMMAND_BUFFER_LEVEL_SECONDARY ||
          !(cmd_buffer->usage_flags & VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT));
#if 0
   genX(cmd_buffer_emit_state_base_address)(cmd_buffer);

   /* We sometimes store vertex data in the dynamic state buffer for blorp
    * operations and our dynamic state stream may re-use data from previous
    * command buffers.  In order to prevent stale cache data, we flush the VF
    * cache.  We could do this on every blorp call but that's not really
    * needed as all of the data will get written by the CPU prior to the GPU
    * executing anything.  The chances are fairly high that they will use
    * blorp at least once per primary command buffer so it shouldn't be
    * wasted.
    */
   if (cmd_buffer->level == VK_COMMAND_BUFFER_LEVEL_PRIMARY)
      cmd_buffer->state.pending_pipe_bits |= ANV_PIPE_VF_CACHE_INVALIDATE_BIT;

   /* We send an "Indirect State Pointers Disable" packet at
    * EndCommandBuffer, so all push contant packets are ignored during a
    * context restore. Documentation says after that command, we need to
    * emit push constants again before any rendering operation. So we
    * flag them dirty here to make sure they get emitted.
    */
   cmd_buffer->state.push_constants_dirty |= VK_SHADER_STAGE_ALL_GRAPHICS;
#endif
   VkResult result = VK_SUCCESS;
   if (cmd_buffer->usage_flags &
       VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT) {
      assert(pBeginInfo->pInheritanceInfo);
      cmd_buffer->state.pass =
         v3dvk_render_pass_from_handle(pBeginInfo->pInheritanceInfo->renderPass);
      cmd_buffer->state.subpass =
         &cmd_buffer->state.pass->subpasses[pBeginInfo->pInheritanceInfo->subpass];

      /* This is optional in the inheritance info. */
      cmd_buffer->state.framebuffer =
         v3dvk_framebuffer_from_handle(pBeginInfo->pInheritanceInfo->framebuffer);

      result = v3dvk_cmd_buffer_setup_attachments(cmd_buffer,
                                                  cmd_buffer->state.pass, NULL);
#if 0
      /* Record that HiZ is enabled if we can. */
      if (cmd_buffer->state.framebuffer) {
         const struct v3dvk_image_view * const iview =
            v3dvk_cmd_buffer_get_depth_stencil_view(cmd_buffer);

         if (iview) {
            VkImageLayout layout =
                cmd_buffer->state.subpass->depth_stencil_attachment->layout;

            enum isl_aux_usage aux_usage =
               anv_layout_to_aux_usage(&cmd_buffer->device->info, iview->image,
                                       VK_IMAGE_ASPECT_DEPTH_BIT, layout);

            cmd_buffer->state.hiz_enabled = aux_usage == ISL_AUX_USAGE_HIZ;
         }
      }

      cmd_buffer->state.gfx.dirty |= ANV_CMD_DIRTY_RENDER_TARGETS;
#endif
   }
#if 0
#if GEN_GEN >= 8 || GEN_IS_HASWELL
   if (cmd_buffer->level == VK_COMMAND_BUFFER_LEVEL_SECONDARY) {
      const VkCommandBufferInheritanceConditionalRenderingInfoEXT *conditional_rendering_info =
         vk_find_struct_const(pBeginInfo->pInheritanceInfo->pNext, COMMAND_BUFFER_INHERITANCE_CONDITIONAL_RENDERING_INFO_EXT);

      /* If secondary buffer supports conditional rendering
       * we should emit commands as if conditional rendering is enabled.
       */
      cmd_buffer->state.conditional_render_enabled =
         conditional_rendering_info && conditional_rendering_info->conditionalRenderingEnable;
   }
#endif
#endif
   return result;
}

void
v3dvk_cmd_buffer_flush_state(struct v3dvk_cmd_buffer *cmd_buffer)
{
   struct v3dvk_pipeline *pipeline = cmd_buffer->state.gfx.base.pipeline;

   if (cmd_buffer->state.gfx.dirty & V3DVK_CMD_DIRTY_XFB_ENABLE) {
      /* We don't need any per-buffer dirty tracking because you're not
       * allowed to bind different XFB buffers while XFB is enabled.
       */
#if 0
     cl_emit(&job->bcl, TRANSFORM_FEEDBACK_SPECS, tfe) {
       tfe.number_of_16_bit_output_data_specs_following =
                                        v3d->prog.bind_vs->num_tf_specs;
       tfe.enable = tf_enabled;
   };

      for (unsigned idx = 0; idx < MAX_XFB_BUFFERS; idx++) {
         struct v3dvk_xfb_binding *xfb = &cmd_buffer->state.xfb_bindings[idx];


         for (int i = 0; i < v3d->prog.bind_vs->num_tf_specs; i++) {
             cl_emit_prepacked(&job->bcl, &tf_specs[i]);
         }

         anv_batch_emit(&cmd_buffer->batch, GENX(3DSTATE_SO_BUFFER), sob) {
            sob.SOBufferIndex = idx;

            if (cmd_buffer->state.xfb_enabled && xfb->buffer && xfb->size != 0) {
               sob.SOBufferEnable = true;
               sob.MOCS = cmd_buffer->device->default_mocs,
               sob.StreamOffsetWriteEnable = false;
               sob.SurfaceBaseAddress = anv_address_add(xfb->buffer->address,
                                                        xfb->offset);
               /* Size is in DWords - 1 */
               sob.SurfaceSize = xfb->size / 4 - 1;
            }
         }
      }
#endif
   }

}

void v3dvk_CmdBeginTransformFeedbackEXT(
    VkCommandBuffer                             commandBuffer,
    uint32_t                                    firstCounterBuffer,
    uint32_t                                    counterBufferCount,
    const VkBuffer*                             pCounterBuffers,
    const VkDeviceSize*                         pCounterBufferOffsets)
{
   V3DVK_FROM_HANDLE(v3dvk_cmd_buffer, cmd_buffer, commandBuffer);

   assert(firstCounterBuffer < MAX_XFB_BUFFERS);
   assert(counterBufferCount <= MAX_XFB_BUFFERS);
   assert(firstCounterBuffer + counterBufferCount <= MAX_XFB_BUFFERS);

#if 0
   /* From the SKL PRM Vol. 2c, SO_WRITE_OFFSET:
    *
    *    "Ssoftware must ensure that no HW stream output operations can be in
    *    process or otherwise pending at the point that the MI_LOAD/STORE
    *    commands are processed. This will likely require a pipeline flush."
    */
   cmd_buffer->state.pending_pipe_bits |= ANV_PIPE_CS_STALL_BIT;
   genX(cmd_buffer_apply_pipe_flushes)(cmd_buffer);
#endif

   for (uint32_t idx = 0; idx < MAX_XFB_BUFFERS; idx++) {
      /* If we have a counter buffer, this is a resume so we need to load the
       * value into the streamout offset register.  Otherwise, this is a begin
       * and we need to reset it to zero.
       */
      if (pCounterBuffers &&
          idx >= firstCounterBuffer &&
          idx - firstCounterBuffer < counterBufferCount &&
          pCounterBuffers[idx - firstCounterBuffer] != VK_NULL_HANDLE) {
         uint32_t cb_idx = idx - firstCounterBuffer;
         V3DVK_FROM_HANDLE(v3dvk_buffer, counter_buffer, pCounterBuffers[cb_idx]);
         uint64_t offset = pCounterBufferOffsets ?
                           pCounterBufferOffsets[cb_idx] : 0;

         v3dvk_batch_emit(&cmd_buffer->batch, GENX(TRANSFORM_FEEDBACK_BUFFER), output) {
            output.buffer_address = cl_address(counter_buffer->address->bo,
                                               counter_buffer->address->offset +
                                                  offset);
            output.buffer_size_in_32_bit_words =
                                        (counter_buffer->size - offset) >> 2;
            output.buffer_number = cb_idx;
         }
      } else {
#if 0
         v3dvk_batch_emit(&cmd_buffer->batch, GENX(MI_LOAD_REGISTER_IMM), lri) {
            lri.RegisterOffset   = GENX(SO_WRITE_OFFSET0_num) + idx * 4;
            lri.DataDWord        = 0;
         }
#endif
         fprintf(stderr, "Unimplemented");
      }
   }

   cmd_buffer->state.xfb_enabled = true;
   cmd_buffer->state.gfx.dirty |= V3DVK_CMD_DIRTY_XFB_ENABLE;
}

void v3dvk_CmdEndTransformFeedbackEXT(
    VkCommandBuffer                             commandBuffer,
    uint32_t                                    firstCounterBuffer,
    uint32_t                                    counterBufferCount,
    const VkBuffer*                             pCounterBuffers,
    const VkDeviceSize*                         pCounterBufferOffsets)
{
   V3DVK_FROM_HANDLE(v3dvk_cmd_buffer, cmd_buffer, commandBuffer);

   assert(firstCounterBuffer < MAX_XFB_BUFFERS);
   assert(counterBufferCount <= MAX_XFB_BUFFERS);
   assert(firstCounterBuffer + counterBufferCount <= MAX_XFB_BUFFERS);

#if 0
   /* From the SKL PRM Vol. 2c, SO_WRITE_OFFSET:
    *
    *    "Ssoftware must ensure that no HW stream output operations can be in
    *    process or otherwise pending at the point that the MI_LOAD/STORE
    *    commands are processed. This will likely require a pipeline flush."
    */
   cmd_buffer->state.pending_pipe_bits |= ANV_PIPE_CS_STALL_BIT;
   genX(cmd_buffer_apply_pipe_flushes)(cmd_buffer);

   for (uint32_t cb_idx = 0; cb_idx < counterBufferCount; cb_idx++) {
      unsigned idx = firstCounterBuffer + cb_idx;

      /* If we have a counter buffer, this is a resume so we need to load the
       * value into the streamout offset register.  Otherwise, this is a begin
       * and we need to reset it to zero.
       */
      if (pCounterBuffers &&
          cb_idx < counterBufferCount &&
          pCounterBuffers[cb_idx] != VK_NULL_HANDLE) {
         V3DVK_FROM_HANDLE(v3dvk_buffer, counter_buffer, pCounterBuffers[cb_idx]);
         uint64_t offset = pCounterBufferOffsets ?
                           pCounterBufferOffsets[cb_idx] : 0;

         v3dvk_batch_emit(&cmd_buffer->batch, GENX(MI_STORE_REGISTER_MEM), srm) {
            srm.MemoryAddress    = anv_address_add(counter_buffer->address,
                                                   offset);
            srm.RegisterAddress  = GENX(SO_WRITE_OFFSET0_num) + idx * 4;
         }
      }
   }

   //V3D42_TRANSFORM_FEEDBACK_SPECS
   cl_emit(&job->bcl, TRANSFORM_FEEDBACK_SPECS, tfe) {
      tfe.enable = false;
   };

   cmd_buffer->state.xfb_enabled = false;
   cmd_buffer->state.gfx.dirty |= V3DVK_CMD_DIRTY_XFB_ENABLE;
#endif
}
