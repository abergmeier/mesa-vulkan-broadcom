
#ifndef V3DVK_DESCRIPTOR_SET_H
#define V3DVK_DESCRIPTOR_SET_H

#include <stdint.h>
#include <vulkan/vulkan.h>
#include "v3dvk_constants.h"

struct v3dvk_descriptor_set_binding_layout
{
   VkDescriptorType type;

   /* Number of array elements in this binding */
   uint32_t array_size;
#if 0
   uint32_t offset;
   uint32_t buffer_offset;
   uint16_t dynamic_offset_offset;

   uint16_t dynamic_offset_count;
   /* redundant with the type, each for a single array element */
   uint32_t size;

   /* Offset in the tu_descriptor_set_layout of the immutable samplers, or 0
    * if there are no immutable samplers. */
   uint32_t immutable_samplers_offset;
#endif
};

struct v3dvk_descriptor_set_layout
{
   /* The create flags for this descriptor set layout */
   VkDescriptorSetLayoutCreateFlags flags;
#if 0

   /* Number of bindings in this descriptor set */
   uint32_t binding_count;

   /* Total size of the descriptor set with room for all array entries */
   uint32_t size;

   /* Shader stages affected by this descriptor set */
   uint16_t shader_stages;
   uint16_t dynamic_shader_stages;

   /* Number of buffers in this descriptor set */
   uint32_t buffer_count;

   /* Number of dynamic offsets used by this descriptor set */
   uint16_t dynamic_offset_count;

   bool has_immutable_samplers;
   bool has_variable_descriptors;

#endif
   /* Bindings in this descriptor set */
   struct v3dvk_descriptor_set_binding_layout binding[0];
};

struct v3dvk_pipeline_layout
{
   struct
   {
      struct v3dvk_descriptor_set_layout *layout;
#if 0
      uint32_t size;
      uint32_t dynamic_offset_start;
#endif
   } set[MAX_SETS];

   uint32_t num_sets;
#if 0
   uint32_t push_constant_size;
   uint32_t dynamic_offset_count;

   unsigned char sha1[20];
#endif
};

#endif
