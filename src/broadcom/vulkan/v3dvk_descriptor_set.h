
#ifndef V3DVK_DESCRIPTOR_SET_H
#define V3DVK_DESCRIPTOR_SET_H

#include <stdint.h>
#include <vulkan/vulkan.h>
#include "v3dvk_bo.h"
#include "v3dvk_constants.h"

struct v3dvk_descriptor_range
{
   uint32_t offset;
   uint32_t size;
};

struct v3dvk_descriptor_set
{
   const struct v3dvk_descriptor_set_layout *layout;
   uint32_t size;

   uint32_t offset;
   uint32_t *mapped_ptr;
   struct v3dvk_descriptor_range *dynamic_descriptors;

   struct v3dvk_bo *descriptors[0];
};

struct v3dvk_descriptor_map
{
   unsigned num;
   int set[32];
   int binding[32];
};

struct v3dvk_descriptor_set_binding_layout
{
   VkDescriptorType type;

   /* Number of array elements in this binding */
   uint32_t array_size;
   uint32_t offset;
   uint32_t buffer_offset;

   uint16_t dynamic_offset_offset;

   uint16_t dynamic_offset_count;

   /* redundant with the type, each for a single array element */
   uint32_t size;

   /* Offset in the v3dvk_descriptor_set_layout of the immutable samplers, or 0
    * if there are no immutable samplers. */
   uint32_t immutable_samplers_offset;

};

struct v3dvk_descriptor_set_layout
{
   /* The create flags for this descriptor set layout */
   VkDescriptorSetLayoutCreateFlags flags;

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

static inline const struct v3dvk_sampler*
v3dvk_immutable_samplers(const struct v3dvk_descriptor_set_layout *set,
                         const struct v3dvk_descriptor_set_binding_layout *binding)
{
   return (struct v3dvk_sampler *) ((const char *) set +
                              binding->immutable_samplers_offset);
}

struct v3dvk_descriptor_pool_entry
{
   uint32_t offset;
   uint32_t size;
   struct v3dvk_descriptor_set *set;
};

struct v3dvk_descriptor_pool
{
   struct v3dvk_bo bo;
   uint64_t current_offset;
   uint64_t size;

   uint8_t *host_memory_base;
   uint8_t *host_memory_ptr;
   uint8_t *host_memory_end;

   uint32_t entry_count;
   uint32_t max_entry_count;
   struct v3dvk_descriptor_pool_entry entries[0];
};

struct v3dvk_descriptor_update_template_entry
{
   VkDescriptorType descriptor_type;

   /* The number of descriptors to update */
   uint32_t descriptor_count;

   /* Into mapped_ptr or dynamic_descriptors, in units of the respective array
    */
   uint32_t dst_offset;

   /* In dwords. Not valid/used for dynamic descriptors */
   uint32_t dst_stride;

   uint32_t buffer_offset;

   /* Only valid for combined image samplers and samplers */
   uint16_t has_sampler;

   /* In bytes */
   size_t src_offset;
   size_t src_stride;

   /* For push descriptors */
   const uint32_t *immutable_samplers;
};

struct v3dvk_descriptor_update_template
{
   uint32_t entry_count;
   VkPipelineBindPoint bind_point;
   struct v3dvk_descriptor_update_template_entry entry[0];
};

#endif
