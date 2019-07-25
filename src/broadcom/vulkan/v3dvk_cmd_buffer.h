
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
#include "v3dvk_batch.h"
#include "v3dvk_defines.h"

struct v3dvk_cmd_pool;
struct v3dvk_device;

struct v3dvk_dynamic_state {
   struct {
      uint32_t                                  count;
      VkViewport                                viewports[MAX_VIEWPORTS];
   } viewport;

   struct {
      uint32_t                                  count;
      VkRect2D                                  scissors[MAX_SCISSORS];
   } scissor;

   float                                        line_width;

   struct {
      float                                     bias;
      float                                     clamp;
      float                                     slope;
   } depth_bias;

   float                                        blend_constants[4];

   struct {
      float                                     min;
      float                                     max;
   } depth_bounds;

   struct {
      uint32_t                                  front;
      uint32_t                                  back;
   } stencil_compare_mask;

   struct {
      uint32_t                                  front;
      uint32_t                                  back;
   } stencil_write_mask;

   struct {
      uint32_t                                  front;
      uint32_t                                  back;
   } stencil_reference;
};

extern const struct v3dvk_dynamic_state default_dynamic_state;

/**
 * Attachment state when recording a renderpass instance.
 *
 * The clear value is valid only if there exists a pending clear.
 */
struct v3dvk_attachment_state {
#if 0
   enum isl_aux_usage                           aux_usage;
   enum isl_aux_usage                           input_aux_usage;
   struct anv_surface_state                     color;
   struct anv_surface_state                     input;

   VkImageLayout                                current_layout;
   VkImageAspectFlags                           pending_clear_aspects;
   VkImageAspectFlags                           pending_load_aspects;
   bool                                         fast_clear;
 #endif
   VkClearValue                                 clear_value;
#if 0
   bool                                         clear_color_is_zero_one;
   bool                                         clear_color_is_zero;

   /* When multiview is active, attachments with a renderpass clear
    * operation have their respective layers cleared on the first
    * subpass that uses them, and only in that subpass. We keep track
    * of this using a bitfield to indicate which layers of an attachment
    * have not been cleared yet when multiview is active.
    */
   uint32_t                                     pending_clear_views;
#endif
};

/** State tracking for particular pipeline bind point
 *
 * This struct is the base struct for v3dvk_cmd_graphics_state and
 * v3dvk_cmd_compute_state.  These are used to track state which is bound to a
 * particular type of pipeline.  Generic state that applies per-stage such as
 * binding table offsets and push constants is tracked generically with a
 * per-stage array in v3dvk_cmd_state.
 */
struct v3dvk_cmd_pipeline_state {
   struct v3dvk_pipeline *pipeline;
   struct v3dvk_pipeline_layout *layout;
#if 0
   struct anv_descriptor_set *descriptors[MAX_SETS];
   uint32_t dynamic_offsets[MAX_DYNAMIC_BUFFERS];

   struct anv_push_descriptor_set *push_descriptors[MAX_SETS];
#endif
};

/** State tracking for graphics pipeline
 *
 * This has v3dvk_cmd_pipeline_state as a base struct to track things which get
 * bound to a graphics pipeline.  Along with general pipeline bind point state
 * which is in the v3dvk_cmd_pipeline_state base struct, it also contains other
 * state which is graphics-specific.
 */
struct v3dvk_cmd_graphics_state {
   struct v3dvk_cmd_pipeline_state base;
#if 0
   anv_cmd_dirty_mask_t dirty;
   uint32_t vb_dirty;
#endif
   struct v3dvk_dynamic_state dynamic;
#if 0
   struct {
      struct anv_buffer *index_buffer;
      uint32_t index_type; /**< 3DSTATE_INDEX_BUFFER.IndexFormat */
      uint32_t index_offset;
   } gen7;
#endif
};

/** State tracking for compute pipeline
 *
 * This has v3dvk_cmd_pipeline_state as a base struct to track things which get
 * bound to a compute pipeline.  Along with general pipeline bind point state
 * which is in the v3dvk_cmd_pipeline_state base struct, it also contains other
 * state which is compute-specific.
 */
struct v3dvk_cmd_compute_state {
   struct v3dvk_cmd_pipeline_state base;
#if 0
   bool pipeline_dirty;

   struct anv_address num_workgroups;
#endif
};

/** State required while building cmd buffer */
struct v3dvk_cmd_state {
   /* PIPELINE_SELECT.PipelineSelection */
   uint32_t                                     current_pipeline;
#if 0
   const struct gen_l3_config *                 current_l3_config;
#endif
   struct v3dvk_cmd_graphics_state              gfx;
   struct v3dvk_cmd_compute_state               compute;
#if 0
   enum anv_pipe_bits                           pending_pipe_bits;
   VkShaderStageFlags                           descriptors_dirty;
   VkShaderStageFlags                           push_constants_dirty;
#endif
   struct v3dvk_framebuffer *                   framebuffer;
   struct v3dvk_render_pass *                   pass;
   struct v3dvk_subpass *                       subpass;
#if 0
   VkRect2D                                     render_area;
   uint32_t                                     restart_index;
   struct anv_vertex_binding                    vertex_bindings[MAX_VBS];
   bool                                         xfb_enabled;
   struct anv_xfb_binding                       xfb_bindings[MAX_XFB_BUFFERS];
   VkShaderStageFlags                           push_constant_stages;
   struct anv_push_constants                    push_constants[MESA_SHADER_STAGES];
   struct anv_state                             binding_tables[MESA_SHADER_STAGES];
   struct anv_state                             samplers[MESA_SHADER_STAGES];

   /**
    * Whether or not the gen8 PMA fix is enabled.  We ensure that, at the top
    * of any command buffer it is disabled by disabling it in EndCommandBuffer
    * and before invoking the secondary in ExecuteCommands.
    */
   bool                                         pma_fix_enabled;

   /**
    * Whether or not we know for certain that HiZ is enabled for the current
    * subpass.  If, for whatever reason, we are unsure as to whether HiZ is
    * enabled or not, this will be false.
    */
   bool                                         hiz_enabled;

   bool                                         conditional_render_enabled;
#endif
   /**
    * Array length is v3dvk_cmd_state::pass::attachment_count. Array content is
    * valid only when recording a render pass instance.
    */
   struct v3dvk_attachment_state *                attachments;
#if 0
   /**
    * Surface states for color render targets.  These are stored in a single
    * flat array.  For depth-stencil attachments, the surface state is simply
    * left blank.
    */
   struct anv_state                             render_pass_states;

   /**
    * A null surface state of the right size to match the framebuffer.  This
    * is one of the states in render_pass_states.
    */
   struct anv_state                             null_surface_state;
#endif
};

struct v3dvk_cmd_buffer {
   VK_LOADER_DATA                               _loader_data;

   struct v3dvk_device *                          device;

   struct v3dvk_cmd_pool *                        pool;
   struct list_head                             pool_link;

   struct v3dvk_batch                           batch;
#if 0
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
#endif
   VkCommandBufferUsageFlags                    usage_flags;
   VkCommandBufferLevel                         level;

   struct v3dvk_cmd_state                       state;
};

void
v3dvk_cmd_buffer_destroy(struct v3dvk_cmd_buffer *cmd_buffer);

const struct v3dvk_image_view *
v3dvk_cmd_buffer_get_depth_stencil_view(const struct v3dvk_cmd_buffer *cmd_buffer);

VkResult v3dvk_cmd_buffer_reset(struct v3dvk_cmd_buffer *cmd_buffer);

struct v3dvk_subpass {
   uint32_t                                     attachment_count;

   /**
    * A pointer to all attachment references used in this subpass.
    * Only valid if ::attachment_count > 0.
    */
   struct v3dvk_subpass_attachment *            attachments;
   uint32_t                                     input_count;
   struct v3dvk_subpass_attachment *            input_attachments;
   uint32_t                                     color_count;
   struct v3dvk_subpass_attachment *            color_attachments;
   struct v3dvk_subpass_attachment *            resolve_attachments;

   struct v3dvk_subpass_attachment *            depth_stencil_attachment;
   struct v3dvk_subpass_attachment *            ds_resolve_attachment;
#if 0
   VkResolveModeFlagBitsKHR                     depth_resolve_mode;
   VkResolveModeFlagBitsKHR                     stencil_resolve_mode;

   uint32_t                                     view_mask;

   /** Subpass has a depth/stencil self-dependency */
   bool                                         has_ds_self_dep;

   /** Subpass has at least one color resolve attachment */
   bool                                         has_color_resolve;
#endif
};

struct v3dvk_render_pass {
   uint32_t                                     attachment_count;
   uint32_t                                     subpass_count;
   /* An array of subpass_count+1 flushes, one per subpass boundary */
#if 0
   enum anv_pipe_bits *                         subpass_flushes;
#endif
   struct v3dvk_render_pass_attachment *        attachments;
   struct v3dvk_subpass                         subpasses[0];
};

VkResult v3dvk_cmd_buffer_execbuf(struct v3dvk_device *device,
                                  struct v3dvk_cmd_buffer *cmd_buffer,
                                  const VkSemaphore *in_semaphores,
                                  uint32_t num_in_semaphores,
                                  const VkSemaphore *out_semaphores,
                                  uint32_t num_out_semaphores,
                                  VkFence fence);

#endif // V3DVK_CMD_BUFFER_H
