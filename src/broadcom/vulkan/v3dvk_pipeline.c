
#include "compiler/shader_enums.h"
#include "vk_alloc.h"
#include "common.h"
#include "device.h"
#include "v3dvk_constants.h"
#include "v3dvk_pass.h"
#include "v3dvk_pipeline.h"
#include "v3dvk_shader.h"

static void
v3dvk_pipeline_finish(struct v3dvk_pipeline *pipeline,
                      struct v3dvk_device *dev,
                      const VkAllocationCallbacks *alloc)
{
#if 0
   tu_cs_finish(dev, &pipeline->cs);

   if (pipeline->program.binary_bo.gem_handle)
      tu_bo_finish(dev, &pipeline->program.binary_bo);
#endif
}

static VkResult
v3dvk_compute_pipeline_create(VkDevice _device,
                              VkPipelineCache _cache,
                              const VkComputePipelineCreateInfo *pCreateInfo,
                              const VkAllocationCallbacks *pAllocator,
                              VkPipeline *pPipeline)
{
   return VK_SUCCESS;
}

VkResult
v3dvk_CreateComputePipelines(VkDevice _device,
                             VkPipelineCache pipelineCache,
                             uint32_t count,
                             const VkComputePipelineCreateInfo *pCreateInfos,
                             const VkAllocationCallbacks *pAllocator,
                             VkPipeline *pPipelines)
{
   VkResult result = VK_SUCCESS;

   for (unsigned i = 0; i < count; i++) {
      VkResult r = v3dvk_compute_pipeline_create(_device, pipelineCache, &pCreateInfos[i],
                                                 pAllocator, &pPipelines[i]);
      if (r != VK_SUCCESS) {
         result = r;
      }
      pPipelines[i] = VK_NULL_HANDLE;
   }

   return result;
}

void
v3dvk_DestroyPipeline(VkDevice _device,
                      VkPipeline _pipeline,
                      const VkAllocationCallbacks *pAllocator)
{
   V3DVK_FROM_HANDLE(v3dvk_device, dev, _device);
   V3DVK_FROM_HANDLE(v3dvk_pipeline, pipeline, _pipeline);

   if (!_pipeline)
      return;

   v3dvk_pipeline_finish(pipeline, dev, pAllocator);
   vk_free2(&dev->alloc, pAllocator, pipeline);
}
