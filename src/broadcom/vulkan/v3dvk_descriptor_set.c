/*
 * Copyright © 2016 Red Hat.
 * Copyright © 2016 Bas Nieuwenhuizen
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
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include <assert.h>
#include <stdlib.h>
#include "common.h"
#include "vk_alloc.h"
#include "vk_util.h"
#include "device.h"
#include "v3dvk_buffer.h"
#include "v3dvk_cmd_buffer.h"
#include "v3dvk_descriptor_set.h"
#include "v3dvk_entrypoints.h"
#include "v3dvk_error.h"
#include "v3dvk_image.h"
#include "v3dvk_log.h"
#include "v3dvk_sampler.h"

static int
binding_compare(const void *av, const void *bv)
{
   const VkDescriptorSetLayoutBinding *a =
      (const VkDescriptorSetLayoutBinding *) av;
   const VkDescriptorSetLayoutBinding *b =
      (const VkDescriptorSetLayoutBinding *) bv;

   return (a->binding < b->binding) ? -1 : (a->binding > b->binding) ? 1 : 0;
}

static VkDescriptorSetLayoutBinding *
create_sorted_bindings(const VkDescriptorSetLayoutBinding *bindings,
                       unsigned count)
{
   VkDescriptorSetLayoutBinding *sorted_bindings =
      malloc(count * sizeof(VkDescriptorSetLayoutBinding));
   if (!sorted_bindings)
      return NULL;

   memcpy(sorted_bindings, bindings,
          count * sizeof(VkDescriptorSetLayoutBinding));

   qsort(sorted_bindings, count, sizeof(VkDescriptorSetLayoutBinding),
         binding_compare);

   return sorted_bindings;
}

static uint32_t
descriptor_size(enum VkDescriptorType type)
{
   switch (type) {
   case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
   case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
      return 0;
   case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
   case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
   case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
   case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
      /* 64bit pointer */
      return 8;
   case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
   case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
      return cl_packet_length(TEXTURE_SHADER_STATE);
   case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
   case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
      // TODO: Implement me
      assert(false);
      return 0;
   case VK_DESCRIPTOR_TYPE_SAMPLER:
      return sizeof(struct v3dvk_sampler);
   default:
      unreachable("unknown descriptor type\n");
      return 0;
   }
}

VkResult
v3dvk_CreateDescriptorSetLayout(
   VkDevice _device,
   const VkDescriptorSetLayoutCreateInfo *pCreateInfo,
   const VkAllocationCallbacks *pAllocator,
   VkDescriptorSetLayout *pSetLayout)
{
   V3DVK_FROM_HANDLE(v3dvk_device, device, _device);
   struct v3dvk_descriptor_set_layout *set_layout;

   assert(pCreateInfo->sType ==
          VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO);
   const VkDescriptorSetLayoutBindingFlagsCreateInfoEXT *variable_flags =
      vk_find_struct_const(
         pCreateInfo->pNext,
         DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO_EXT);

   uint32_t max_binding = 0;
   uint32_t immutable_sampler_count = 0;
   for (uint32_t j = 0; j < pCreateInfo->bindingCount; j++) {
      max_binding = MAX2(max_binding, pCreateInfo->pBindings[j].binding);
      if ((pCreateInfo->pBindings[j].descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER ||
           pCreateInfo->pBindings[j].descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER) &&
           pCreateInfo->pBindings[j].pImmutableSamplers) {
         immutable_sampler_count += pCreateInfo->pBindings[j].descriptorCount;
      }
   }

   uint32_t samplers_offset = sizeof(struct v3dvk_descriptor_set_layout) +
      (max_binding + 1) * sizeof(set_layout->binding[0]);
   uint32_t size = samplers_offset + immutable_sampler_count * sizeof(struct v3dvk_sampler);

   set_layout = vk_alloc2(&device->alloc, pAllocator, size, 8,
                          VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
   if (!set_layout)
      return v3dvk_error(device->instance, VK_ERROR_OUT_OF_HOST_MEMORY);

   set_layout->flags = pCreateInfo->flags;

   /* We just allocate all the samplers at the end of the struct */
   struct v3dvk_sampler *samplers = (void*) &set_layout->binding[max_binding + 1];

   VkDescriptorSetLayoutBinding *bindings = create_sorted_bindings(
      pCreateInfo->pBindings, pCreateInfo->bindingCount);
   if (!bindings) {
      vk_free2(&device->alloc, pAllocator, set_layout);
      return v3dvk_error(device->instance, VK_ERROR_OUT_OF_HOST_MEMORY);
   }

   set_layout->binding_count = max_binding + 1;
   set_layout->shader_stages = 0;
   set_layout->dynamic_shader_stages = 0;
   set_layout->has_immutable_samplers = false;
   set_layout->size = 0;

   memset(set_layout->binding, 0,
          size - sizeof(struct v3dvk_descriptor_set_layout));

   uint32_t buffer_count = 0;
   uint32_t dynamic_offset_count = 0;

   for (uint32_t j = 0; j < pCreateInfo->bindingCount; j++) {
      const VkDescriptorSetLayoutBinding *binding = bindings + j;
      uint32_t b = binding->binding;
      uint32_t alignment = 8;
      unsigned binding_buffer_count = 1;

      switch (binding->descriptorType) {
      case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
      case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
         assert(!(pCreateInfo->flags & VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR));
         set_layout->binding[b].dynamic_offset_count = 1;
         break;
      default:
         break;
      }

      set_layout->size = align(set_layout->size, alignment);
      set_layout->binding[b].type = binding->descriptorType;
      set_layout->binding[b].array_size = binding->descriptorCount;
      set_layout->binding[b].offset = set_layout->size;
      set_layout->binding[b].buffer_offset = buffer_count;
      set_layout->binding[b].dynamic_offset_offset = dynamic_offset_count;
      set_layout->binding[b].size = descriptor_size(binding->descriptorType);

      if (variable_flags && binding->binding < variable_flags->bindingCount &&
          (variable_flags->pBindingFlags[binding->binding] &
           VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT_EXT)) {
         assert(!binding->pImmutableSamplers); /* Terribly ill defined  how
                                                  many samplers are valid */
         assert(binding->binding == max_binding);

         set_layout->has_variable_descriptors = true;
      }

      if ((binding->descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER ||
           binding->descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER) &&
          binding->pImmutableSamplers) {
         set_layout->binding[b].immutable_samplers_offset = samplers_offset;
         set_layout->has_immutable_samplers = true;

         for (uint32_t i = 0; i < binding->descriptorCount; i++)
            samplers[i] = *v3dvk_sampler_from_handle(binding->pImmutableSamplers[i]);

         samplers += binding->descriptorCount;
         samplers_offset += sizeof(struct v3dvk_sampler) * binding->descriptorCount;
      }

      set_layout->size +=
         binding->descriptorCount * set_layout->binding[b].size;
      buffer_count += binding->descriptorCount * binding_buffer_count;
      dynamic_offset_count += binding->descriptorCount *
                              set_layout->binding[b].dynamic_offset_count;
      set_layout->shader_stages |= binding->stageFlags;
   }

   free(bindings);

   set_layout->buffer_count = buffer_count;
   set_layout->dynamic_offset_count = dynamic_offset_count;

   *pSetLayout = v3dvk_descriptor_set_layout_to_handle(set_layout);

   return VK_SUCCESS;
}

void
v3dvk_DestroyDescriptorSetLayout(VkDevice _device,
                                 VkDescriptorSetLayout _set_layout,
                                 const VkAllocationCallbacks *pAllocator)
{
   V3DVK_FROM_HANDLE(v3dvk_device, device, _device);
   V3DVK_FROM_HANDLE(v3dvk_descriptor_set_layout, set_layout, _set_layout);

   if (!set_layout)
      return;

   vk_free2(&device->alloc, pAllocator, set_layout);
}


/*
 * Pipeline layouts.  These have nothing to do with the pipeline.  They are
 * just multiple descriptor set layouts pasted together.
 */

VkResult
v3dvk_CreatePipelineLayout(VkDevice _device,
                           const VkPipelineLayoutCreateInfo *pCreateInfo,
                           const VkAllocationCallbacks *pAllocator,
                           VkPipelineLayout *pPipelineLayout)
{
   V3DVK_FROM_HANDLE(v3dvk_device, device, _device);
   struct v3dvk_pipeline_layout *layout;
#if 0
   struct mesa_sha1 ctx;
#endif
   assert(pCreateInfo->sType ==
          VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO);

   layout = vk_alloc2(&device->alloc, pAllocator, sizeof(*layout), 8,
                      VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
   if (layout == NULL)
      return v3dvk_error(device->instance, VK_ERROR_OUT_OF_HOST_MEMORY);
#if 0
   layout->num_sets = pCreateInfo->setLayoutCount;

   unsigned dynamic_offset_count = 0;

   _mesa_sha1_init(&ctx);
   for (uint32_t set = 0; set < pCreateInfo->setLayoutCount; set++) {
      TU_FROM_HANDLE(tu_descriptor_set_layout, set_layout,
                     pCreateInfo->pSetLayouts[set]);
      layout->set[set].layout = set_layout;

      layout->set[set].dynamic_offset_start = dynamic_offset_count;
      for (uint32_t b = 0; b < set_layout->binding_count; b++) {
         dynamic_offset_count += set_layout->binding[b].array_size *
                                 set_layout->binding[b].dynamic_offset_count;
         if (set_layout->binding[b].immutable_samplers_offset)
            _mesa_sha1_update(
               &ctx,
               tu_immutable_samplers(set_layout, set_layout->binding + b),
               set_layout->binding[b].array_size * 4 * sizeof(uint32_t));
      }
      _mesa_sha1_update(
         &ctx, set_layout->binding,
         sizeof(set_layout->binding[0]) * set_layout->binding_count);
   }

   layout->dynamic_offset_count = dynamic_offset_count;
   layout->push_constant_size = 0;

   for (unsigned i = 0; i < pCreateInfo->pushConstantRangeCount; ++i) {
      const VkPushConstantRange *range = pCreateInfo->pPushConstantRanges + i;
      layout->push_constant_size =
         MAX2(layout->push_constant_size, range->offset + range->size);
   }

   layout->push_constant_size = align(layout->push_constant_size, 16);
   _mesa_sha1_update(&ctx, &layout->push_constant_size,
                     sizeof(layout->push_constant_size));
   _mesa_sha1_final(&ctx, layout->sha1);
#endif
   *pPipelineLayout = v3dvk_pipeline_layout_to_handle(layout);

   return VK_SUCCESS;
}

void
v3dvk_DestroyPipelineLayout(VkDevice _device,
                            VkPipelineLayout _pipelineLayout,
                            const VkAllocationCallbacks *pAllocator)
{
   V3DVK_FROM_HANDLE(v3dvk_device, device, _device);
   V3DVK_FROM_HANDLE(v3dvk_pipeline_layout, pipeline_layout, _pipelineLayout);

   if (!pipeline_layout)
      return;
   vk_free2(&device->alloc, pAllocator, pipeline_layout);
}

static VkResult
v3dvk_descriptor_set_create(struct v3dvk_device *device,
                            struct v3dvk_descriptor_pool *pool,
                            const struct v3dvk_descriptor_set_layout *layout,
                            const uint32_t *variable_count,
                            struct v3dvk_descriptor_set **out_set)
{
   struct v3dvk_descriptor_set *set;
   uint32_t buffer_count = layout->buffer_count;
   if (variable_count) {
      unsigned stride = 1;
      if (layout->binding[layout->binding_count - 1].type == VK_DESCRIPTOR_TYPE_SAMPLER ||
          layout->binding[layout->binding_count - 1].type == VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT)
         stride = 0;
      buffer_count = layout->binding[layout->binding_count - 1].buffer_offset +
                     *variable_count * stride;
   }
   unsigned range_offset = sizeof(struct v3dvk_descriptor_set) +
      sizeof(struct v3dvk_bo *) * buffer_count;
   unsigned mem_size = range_offset +
      sizeof(struct v3dvk_descriptor_range) * layout->dynamic_offset_count;

   if (pool->host_memory_base) {
      if (pool->host_memory_end - pool->host_memory_ptr < mem_size)
         return v3dvk_error(device->instance, VK_ERROR_OUT_OF_POOL_MEMORY);

      set = (struct v3dvk_descriptor_set*)pool->host_memory_ptr;
      pool->host_memory_ptr += mem_size;
   } else {
      set = vk_alloc2(&device->alloc, NULL, mem_size, 8,
                      VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);

      if (!set)
         return v3dvk_error(device->instance, VK_ERROR_OUT_OF_HOST_MEMORY);
   }

   memset(set, 0, mem_size);

   if (layout->dynamic_offset_count) {
      set->dynamic_descriptors = (struct v3dvk_descriptor_range*)((uint8_t*)set + range_offset);
   }

   set->layout = layout;
   uint32_t layout_size = layout->size;
   fprintf(stderr, "DS: Layout %d\n", layout_size);
   if (variable_count) {
      assert(layout->has_variable_descriptors);
      uint32_t stride = layout->binding[layout->binding_count - 1].size;
      if (layout->binding[layout->binding_count - 1].type == VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT)
         stride = 1;

      layout_size = layout->binding[layout->binding_count - 1].offset +
                    *variable_count * stride;
      fprintf(stderr, "DS: Layout mod %d\n", layout_size);
   }

   if (layout_size) {
      set->size = layout_size;

      if (!pool->host_memory_base && pool->entry_count == pool->max_entry_count) {
         vk_free2(&device->alloc, NULL, set);
         return v3dvk_error(device->instance, VK_ERROR_OUT_OF_POOL_MEMORY);
      }

      fprintf(stderr, "DS: Offset %llu %llu\n", pool->current_offset, pool->size);

      /* try to allocate linearly first, so that we don't spend
       * time looking for gaps if the app only allocates &
       * resets via the pool. */
      if (pool->current_offset + layout_size <= pool->size) {
         set->mapped_ptr = (uint32_t*)(pool->bo.map + pool->current_offset);
         set->offset = pool->bo.offset + pool->current_offset;

         if (!pool->host_memory_base) {
            pool->entries[pool->entry_count].offset = pool->current_offset;
            pool->entries[pool->entry_count].size = layout_size;
            pool->entries[pool->entry_count].set = set;
            pool->entry_count++;
         }
         pool->current_offset += layout_size;
      } else if (!pool->host_memory_base) {
         uint64_t offset = 0;
         int index;

         fprintf(stderr, "DS: Entry %d\n", pool->entry_count);

         for (index = 0; index < pool->entry_count; ++index) {
            if (pool->entries[index].offset - offset >= layout_size)
               break;
            offset = pool->entries[index].offset + pool->entries[index].size;
            fprintf(stderr, "DS: Entry Offset %llu\n", offset);
         }

         if (pool->size - offset < layout_size) {
            vk_free2(&device->alloc, NULL, set);
            assert(false);
            return v3dvk_error(device->instance, VK_ERROR_OUT_OF_POOL_MEMORY);
         }

         set->mapped_ptr = (uint32_t*)(pool->bo.map + offset);
         set->offset = pool->bo.offset + offset;

         memmove(&pool->entries[index + 1], &pool->entries[index],
            sizeof(pool->entries[0]) * (pool->entry_count - index));
         pool->entries[index].offset = offset;
         pool->entries[index].size = layout_size;
         pool->entries[index].set = set;
         pool->entry_count++;
      } else
         return v3dvk_error(device->instance, VK_ERROR_OUT_OF_POOL_MEMORY);
   }

   *out_set = set;
   return VK_SUCCESS;
}

static void
v3dvk_descriptor_set_destroy(struct v3dvk_device *device,
                             struct v3dvk_descriptor_pool *pool,
                             struct v3dvk_descriptor_set *set,
                             bool free_bo)
{
   assert(!pool->host_memory_base);

   if (free_bo && set->size && !pool->host_memory_base) {
      uint32_t offset = (uint8_t*)set->mapped_ptr - (uint8_t*)pool->bo.map;
      for (int i = 0; i < pool->entry_count; ++i) {
         if (pool->entries[i].offset == offset) {
            memmove(&pool->entries[i], &pool->entries[i+1],
               sizeof(pool->entries[i]) * (pool->entry_count - i - 1));
            --pool->entry_count;
            break;
         }
      }
   }
   vk_free2(&device->alloc, NULL, set);
}

VkResult
v3dvk_CreateDescriptorPool(VkDevice _device,
                           const VkDescriptorPoolCreateInfo *pCreateInfo,
                           const VkAllocationCallbacks *pAllocator,
                           VkDescriptorPool *pDescriptorPool)
{
   V3DVK_FROM_HANDLE(v3dvk_device, device, _device);
   struct v3dvk_descriptor_pool *pool;
   uint64_t size = sizeof(struct v3dvk_descriptor_pool);
   uint64_t bo_size = 0, bo_count = 0, range_count = 0;

   for (unsigned i = 0; i < pCreateInfo->poolSizeCount; ++i) {
      if (pCreateInfo->pPoolSizes[i].type != VK_DESCRIPTOR_TYPE_SAMPLER)
         bo_count += pCreateInfo->pPoolSizes[i].descriptorCount;

      switch(pCreateInfo->pPoolSizes[i].type) {
      case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
      case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
         range_count += pCreateInfo->pPoolSizes[i].descriptorCount;
      default:
         break;
      }

      bo_size += descriptor_size(pCreateInfo->pPoolSizes[i].type) *
                           pCreateInfo->pPoolSizes[i].descriptorCount;
   }

   if (!(pCreateInfo->flags & VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT)) {
      uint64_t host_size = pCreateInfo->maxSets * sizeof(struct v3dvk_descriptor_set);
      host_size += sizeof(struct v3dvk_bo*) * bo_count;
      host_size += sizeof(struct v3dvk_descriptor_range) * range_count;
      size += host_size;
   } else {
      size += sizeof(struct v3dvk_descriptor_pool_entry) * pCreateInfo->maxSets;
   }

   pool = vk_alloc2(&device->alloc, pAllocator, size, 8,
                     VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
   if (!pool)
      return v3dvk_error(device->instance, VK_ERROR_OUT_OF_HOST_MEMORY);

   memset(pool, 0, sizeof(*pool));

   if (!(pCreateInfo->flags & VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT)) {
      pool->host_memory_base = (uint8_t*)pool + sizeof(struct v3dvk_descriptor_pool);
      pool->host_memory_ptr = pool->host_memory_base;
      pool->host_memory_end = (uint8_t*)pool + size;
   }

   if (bo_size) {
      VkResult ret;

      fprintf(stderr, "POOL SIZE %llu\n", bo_size);
      ret = v3dvk_bo_init_new(device, &pool->bo, bo_size, "pool");
      assert(ret == VK_SUCCESS);

      v3dvk_bo_map(&pool->bo);
   }
   pool->size = bo_size;
   pool->max_entry_count = pCreateInfo->maxSets;

   *pDescriptorPool = v3dvk_descriptor_pool_to_handle(pool);
   return VK_SUCCESS;
}

void
v3dvk_DestroyDescriptorPool(VkDevice _device,
                            VkDescriptorPool _pool,
                            const VkAllocationCallbacks *pAllocator)
{
   V3DVK_FROM_HANDLE(v3dvk_device, device, _device);
   V3DVK_FROM_HANDLE(v3dvk_descriptor_pool, pool, _pool);

   if (!pool)
      return;
#if 0
   if (!pool->host_memory_base) {
      for(int i = 0; i < pool->entry_count; ++i) {
         v3dvk_descriptor_set_destroy(device, pool, pool->entries[i].set, false);
      }
   }
#endif
   if (pool->size)
      v3dvk_bo_finish(device, &pool->bo);
   vk_free2(&device->alloc, pAllocator, pool);
}

VkResult
v3dvk_AllocateDescriptorSets(VkDevice _device,
                             const VkDescriptorSetAllocateInfo *pAllocateInfo,
                             VkDescriptorSet *pDescriptorSets)
{
   V3DVK_FROM_HANDLE(v3dvk_device, device, _device);
   V3DVK_FROM_HANDLE(v3dvk_descriptor_pool, pool, pAllocateInfo->descriptorPool);

   VkResult result = VK_SUCCESS;
   uint32_t i;
   struct v3dvk_descriptor_set *set = NULL;

   const VkDescriptorSetVariableDescriptorCountAllocateInfoEXT *variable_counts =
      vk_find_struct_const(pAllocateInfo->pNext, DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO_EXT);
   const uint32_t zero = 0;

   /* allocate a set of buffers for each shader to contain descriptors */
   for (i = 0; i < pAllocateInfo->descriptorSetCount; i++) {
      V3DVK_FROM_HANDLE(v3dvk_descriptor_set_layout, layout,
             pAllocateInfo->pSetLayouts[i]);

      const uint32_t *variable_count = NULL;
      if (variable_counts) {
         if (i < variable_counts->descriptorSetCount)
            variable_count = variable_counts->pDescriptorCounts + i;
         else
            variable_count = &zero;
      }

      assert(!(layout->flags & VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR));

      result = v3dvk_descriptor_set_create(device, pool, layout, variable_count, &set);
      if (result != VK_SUCCESS)
         break;

      pDescriptorSets[i] = v3dvk_descriptor_set_to_handle(set);
   }

   if (result != VK_SUCCESS) {
      v3dvk_FreeDescriptorSets(_device, pAllocateInfo->descriptorPool,
               i, pDescriptorSets);
      for (i = 0; i < pAllocateInfo->descriptorSetCount; i++) {
         pDescriptorSets[i] = VK_NULL_HANDLE;
      }
   }
   return result;
}

VkResult
v3dvk_FreeDescriptorSets(VkDevice _device,
                         VkDescriptorPool descriptorPool,
                         uint32_t count,
                         const VkDescriptorSet *pDescriptorSets)
{
   V3DVK_FROM_HANDLE(v3dvk_device, device, _device);
   V3DVK_FROM_HANDLE(v3dvk_descriptor_pool, pool, descriptorPool);

   for (uint32_t i = 0; i < count; i++) {
      V3DVK_FROM_HANDLE(v3dvk_descriptor_set, set, pDescriptorSets[i]);

      if (set && !pool->host_memory_base)
         v3dvk_descriptor_set_destroy(device, pool, set, true);
   }
   return VK_SUCCESS;
}

static void write_texel_buffer_descriptor(struct v3dvk_device *device,
                                          struct v3dvk_cmd_buffer *cmd_buffer,
                                          unsigned *dst,
                                          struct v3dvk_bo **buffer_list,
                                          const VkBufferView _buffer_view)
{
   V3DVK_FROM_HANDLE(v3dvk_buffer_view, buffer_view, _buffer_view);

   V3DVK_FINISHME("texel buffer descriptor");
}

static void write_buffer_descriptor(struct v3dvk_device *device,
                                    struct v3dvk_cmd_buffer *cmd_buffer,
                                    uint32_t *dst,
                                    struct v3dvk_bo **buffer_list,
                                    const VkDescriptorBufferInfo *buffer_info)
{
   V3DVK_FROM_HANDLE(v3dvk_buffer, buffer, buffer_info->buffer);

   uint64_t va = buffer->bo->offset;

   va += buffer_info->offset + buffer->bo_offset;
   if (unlikely(device->robust_buffer_access && va >= UINT32_MAX)) {
      fprintf(stderr, "V3D currently only handles 32bit addresses");
      abort();
   }
   *dst = (uint32_t)va;

#if 0
   if (cmd_buffer)
      v3dvk_bo_list_add(&cmd_buffer->bo_list, buffer->bo, MSM_SUBMIT_BO_READ);
   else
#endif
      *buffer_list = buffer->bo;
}

static void write_dynamic_buffer_descriptor(struct v3dvk_device *device,
                                            struct v3dvk_descriptor_range *range,
                                            struct v3dvk_bo **buffer_list,
                                            const VkDescriptorBufferInfo *buffer_info)
{
   V3DVK_FROM_HANDLE(v3dvk_buffer, buffer, buffer_info->buffer);

   uint64_t va = buffer->bo->offset;
   unsigned size = buffer_info->range;

   if (buffer_info->range == VK_WHOLE_SIZE)
      size = buffer->size - buffer_info->offset;

   va += buffer_info->offset + buffer->bo_offset;
   if (unlikely(device->robust_buffer_access && va >= UINT32_MAX)) {
      fprintf(stderr, "V3D currently only handles 32bit addresses");
      abort();
   }
   range->offset = va;
   range->size = size;

   *buffer_list = buffer->bo;
}

static void
write_image_descriptor(struct v3dvk_device *device,
             struct v3dvk_cmd_buffer *cmd_buffer,
             uint32_t *dst,
             struct v3dvk_bo **buffer_list,
             VkDescriptorType descriptor_type,
             const VkDescriptorImageInfo *image_info)
{
   V3DVK_FROM_HANDLE(v3dvk_image_view, iview, image_info->imageView);
   uint32_t *descriptor;

   if (descriptor_type == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE) {
      descriptor = iview->storage_descriptor;
   } else {
      descriptor = iview->descriptor;
   }

   memcpy(dst, descriptor, sizeof(iview->descriptor));

#if 0
   if (cmd_buffer)
      v3dvk_bo_list_add(&cmd_buffer->bo_list, iview->image->bo, MSM_SUBMIT_BO_READ);
   else
#endif
      *buffer_list = iview->image->bo;
}

static void
write_combined_image_sampler_descriptor(struct v3dvk_device *device,
               struct v3dvk_cmd_buffer *cmd_buffer,
               unsigned sampler_offset,
               uint32_t *dst,
               struct v3dvk_bo **buffer_list,
               VkDescriptorType descriptor_type,
               const VkDescriptorImageInfo *image_info,
               bool has_sampler)
{
   V3DVK_FROM_HANDLE(v3dvk_sampler, sampler, image_info->sampler);

   write_image_descriptor(device, cmd_buffer, dst, buffer_list,
                          descriptor_type, image_info);
   /* copy over sampler state */
   if (has_sampler) {
      memcpy(dst + sampler_offset / sizeof(*dst), sampler, sizeof(*sampler));
   }
}

static void
write_sampler_descriptor(struct v3dvk_device *device,
                         unsigned *dst,
                         const VkDescriptorImageInfo *image_info)
{
   V3DVK_FROM_HANDLE(v3dvk_sampler, sampler, image_info->sampler);

   memcpy(dst, sampler, sizeof(*sampler));
}

static void
v3dvk_update_descriptor_set_with_template(
   struct v3dvk_device *device,
   struct v3dvk_cmd_buffer *cmd_buffer,
   struct v3dvk_descriptor_set *set,
   VkDescriptorUpdateTemplate descriptorUpdateTemplate,
   const void *pData)
{
   V3DVK_FROM_HANDLE(v3dvk_descriptor_update_template, templ,
                     descriptorUpdateTemplate);
   V3DVK_STUB();
}

static void
v3dvk_update_descriptor_sets(struct v3dvk_device *device,
                             struct v3dvk_cmd_buffer *cmd_buffer,
                             VkDescriptorSet dstSetOverride,
                             uint32_t descriptorWriteCount,
                             const VkWriteDescriptorSet *pDescriptorWrites,
                             uint32_t descriptorCopyCount,
                             const VkCopyDescriptorSet *pDescriptorCopies)
{
   uint32_t i, j;
   for (i = 0; i < descriptorWriteCount; i++) {
      const VkWriteDescriptorSet *writeset = &pDescriptorWrites[i];
      V3DVK_FROM_HANDLE(v3dvk_descriptor_set, set,
                        dstSetOverride ? dstSetOverride : writeset->dstSet);
      const struct v3dvk_descriptor_set_binding_layout *binding_layout =
         set->layout->binding + writeset->dstBinding;
      uint32_t *ptr = set->mapped_ptr;
      struct v3dvk_bo **buffer_list = set->descriptors;

      const struct v3dvk_sampler *samplers = v3dvk_immutable_samplers(set->layout, binding_layout);

      ptr += binding_layout->offset / 4;

      ptr += binding_layout->size * writeset->dstArrayElement / 4;
      buffer_list += binding_layout->buffer_offset;
      buffer_list += writeset->dstArrayElement;
      for (j = 0; j < writeset->descriptorCount; ++j) {
         switch(writeset->descriptorType) {
         case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
         case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC: {
            unsigned idx = writeset->dstArrayElement + j;
            idx += binding_layout->dynamic_offset_offset;
            assert(!(set->layout->flags & VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR));
            write_dynamic_buffer_descriptor(device, set->dynamic_descriptors + idx,
                        buffer_list, writeset->pBufferInfo + j);
            break;
         }

         case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
         case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
            write_buffer_descriptor(device, cmd_buffer, ptr, buffer_list,
                     writeset->pBufferInfo + j);
            break;
         case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
         case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
            write_texel_buffer_descriptor(device, cmd_buffer, ptr, buffer_list,
                     writeset->pTexelBufferView[j]);
            break;
         case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
         case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
         case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
            write_image_descriptor(device, cmd_buffer, ptr, buffer_list,
                                   writeset->descriptorType,
                                   writeset->pImageInfo + j);
            break;
         case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER: {
            // TODO: Check whether this makes sense
            const unsigned sampler_offset = 0;
            write_combined_image_sampler_descriptor(device, cmd_buffer,
                                                    sampler_offset,
                                                    ptr, buffer_list,
                                                    writeset->descriptorType,
                                                    writeset->pImageInfo + j,
                                                    !binding_layout->immutable_samplers_offset);
            if (binding_layout->immutable_samplers_offset) {
               const unsigned idx = writeset->dstArrayElement + j;
               memcpy((char*)ptr + sampler_offset, &samplers[idx],
                      sizeof(struct v3dvk_sampler));
            }
            break;
         }
         case VK_DESCRIPTOR_TYPE_SAMPLER:
            write_sampler_descriptor(device, ptr, writeset->pImageInfo + j);
            break;
         default:
            unreachable("unimplemented descriptor type");
            break;
         }
         ptr += binding_layout->size / 4;
         ++buffer_list;
      }
   }

   for (i = 0; i < descriptorCopyCount; i++) {
      const VkCopyDescriptorSet *copyset = &pDescriptorCopies[i];
      V3DVK_FROM_HANDLE(v3dvk_descriptor_set, src_set,
                        copyset->srcSet);
      V3DVK_FROM_HANDLE(v3dvk_descriptor_set, dst_set,
                        copyset->dstSet);
      const struct v3dvk_descriptor_set_binding_layout *src_binding_layout =
         src_set->layout->binding + copyset->srcBinding;
      const struct v3dvk_descriptor_set_binding_layout *dst_binding_layout =
         dst_set->layout->binding + copyset->dstBinding;
      uint32_t *src_ptr = src_set->mapped_ptr;
      uint32_t *dst_ptr = dst_set->mapped_ptr;
      struct v3dvk_bo **src_buffer_list = src_set->descriptors;
      struct v3dvk_bo **dst_buffer_list = dst_set->descriptors;

      src_ptr += src_binding_layout->offset / 4;
      dst_ptr += dst_binding_layout->offset / 4;

      src_ptr += src_binding_layout->size * copyset->srcArrayElement / 4;
      dst_ptr += dst_binding_layout->size * copyset->dstArrayElement / 4;

      src_buffer_list += src_binding_layout->buffer_offset;
      src_buffer_list += copyset->srcArrayElement;

      dst_buffer_list += dst_binding_layout->buffer_offset;
      dst_buffer_list += copyset->dstArrayElement;

      for (j = 0; j < copyset->descriptorCount; ++j) {
         switch (src_binding_layout->type) {
         case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
         case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC: {
            unsigned src_idx = copyset->srcArrayElement + j;
            unsigned dst_idx = copyset->dstArrayElement + j;
            struct v3dvk_descriptor_range *src_range, *dst_range;
            src_idx += src_binding_layout->dynamic_offset_offset;
            dst_idx += dst_binding_layout->dynamic_offset_offset;

            src_range = src_set->dynamic_descriptors + src_idx;
            dst_range = dst_set->dynamic_descriptors + dst_idx;
            *dst_range = *src_range;
            break;
         }
         default:
            memcpy(dst_ptr, src_ptr, src_binding_layout->size);
         }
         src_ptr += src_binding_layout->size / 4;
         dst_ptr += dst_binding_layout->size / 4;

         if (src_binding_layout->type != VK_DESCRIPTOR_TYPE_SAMPLER) {
            /* Sampler descriptors don't have a buffer list. */
            dst_buffer_list[j] = src_buffer_list[j];
         }
      }
   }
}

void
v3dvk_UpdateDescriptorSets(VkDevice _device,
                           uint32_t descriptorWriteCount,
                           const VkWriteDescriptorSet *pDescriptorWrites,
                           uint32_t descriptorCopyCount,
                           const VkCopyDescriptorSet *pDescriptorCopies)
{
   V3DVK_FROM_HANDLE(v3dvk_device, device, _device);

   v3dvk_update_descriptor_sets(device, NULL, VK_NULL_HANDLE,
                                descriptorWriteCount, pDescriptorWrites,
                                descriptorCopyCount, pDescriptorCopies);
}

VkResult
v3dvk_CreateDescriptorUpdateTemplate(
   VkDevice _device,
   const VkDescriptorUpdateTemplateCreateInfo *pCreateInfo,
   const VkAllocationCallbacks *pAllocator,
   VkDescriptorUpdateTemplate *pDescriptorUpdateTemplate)
{
   V3DVK_FROM_HANDLE(v3dvk_device, device, _device);
   V3DVK_FROM_HANDLE(v3dvk_descriptor_set_layout, set_layout,
                     pCreateInfo->descriptorSetLayout);
   const uint32_t entry_count = pCreateInfo->descriptorUpdateEntryCount;
   const size_t size =
      sizeof(struct v3dvk_descriptor_update_template) +
      sizeof(struct v3dvk_descriptor_update_template_entry) * entry_count;
   struct v3dvk_descriptor_update_template *templ;

   templ = vk_alloc2(&device->alloc, pAllocator, size, 8,
                     VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
   if (!templ)
      return v3dvk_error(device->instance, VK_ERROR_OUT_OF_HOST_MEMORY);

   *pDescriptorUpdateTemplate =
      v3dvk_descriptor_update_template_to_handle(templ);

   V3DVK_STUB();
   return VK_SUCCESS;
}

void
v3dvk_DestroyDescriptorUpdateTemplate(
   VkDevice _device,
   VkDescriptorUpdateTemplate descriptorUpdateTemplate,
   const VkAllocationCallbacks *pAllocator)
{
   V3DVK_FROM_HANDLE(v3dvk_device, device, _device);
   V3DVK_FROM_HANDLE(v3dvk_descriptor_update_template, templ,
                     descriptorUpdateTemplate);

   if (!templ)
      return;

   vk_free2(&device->alloc, pAllocator, templ);
}

void
v3dvk_UpdateDescriptorSetWithTemplate(
   VkDevice _device,
   VkDescriptorSet descriptorSet,
   VkDescriptorUpdateTemplate descriptorUpdateTemplate,
   const void *pData)
{
   V3DVK_FROM_HANDLE(v3dvk_device, device, _device);
   V3DVK_FROM_HANDLE(v3dvk_descriptor_set, set, descriptorSet);

   v3dvk_update_descriptor_set_with_template(device, NULL, set,
                                             descriptorUpdateTemplate, pData);
}
