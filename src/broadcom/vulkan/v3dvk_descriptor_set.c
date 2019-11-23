
#include <assert.h>
#include "common.h"
#include "vk_alloc.h"
#include "device.h"
#include "v3dvk_descriptor_set.h"
#include "v3dvk_entrypoints.h"
#include "v3dvk_error.h"


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
