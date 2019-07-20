
#ifndef V3DVK_PASS_H
#define V3DVK_PASS_H

#include <stdbool.h>
#include <vulkan/vulkan.h>

struct v3dvk_subpass_attachment
{
   uint32_t attachment;
   VkImageLayout layout;
};

struct v3dvk_subpass
{
   uint32_t input_count;
   uint32_t color_count;
   struct v3dvk_subpass_attachment *input_attachments;
   struct v3dvk_subpass_attachment *color_attachments;
   struct v3dvk_subpass_attachment *resolve_attachments;
   struct v3dvk_subpass_attachment depth_stencil_attachment;

   /** Subpass has at least one resolve attachment */
   bool has_resolve;
#if 0
   struct tu_subpass_barrier start_barrier;
#endif
   uint32_t view_mask;
   VkSampleCountFlagBits max_sample_count;
};

struct v3dvk_render_pass_attachment
{
   VkFormat format;
   uint32_t samples;
   VkAttachmentLoadOp load_op;
   VkAttachmentLoadOp stencil_load_op;
   VkImageLayout initial_layout;
   VkImageLayout final_layout;
   uint32_t view_mask;
};

struct v3dvk_render_pass
{
   uint32_t attachment_count;
   uint32_t subpass_count;
   struct v3dvk_subpass_attachment *subpass_attachments;
   struct v3dvk_render_pass_attachment *attachments;
#if 0
   struct tu_subpass_barrier end_barrier;
#endif
   struct v3dvk_subpass subpasses[0];
};

#endif
