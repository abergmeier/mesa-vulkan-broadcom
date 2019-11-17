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

#include <fcntl.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <vulkan/vulkan_core.h>
#include "util/debug.h"
#include "vulkan/util/vk_util.h"
#include "common.h"
#include "device.h"
#include "instance.h"
#include "v3dvk_buffer.h"
#include "v3dvk_error.h"
#include "v3dvk_gem.h"
#include "v3dvk_physical_device.h"

VkResult
_v3dvk_device_set_lost(struct v3dvk_device *device,
                       const char *file, int line,
                       const char *msg, ...)
{
   VkResult err;
   va_list ap;

   device->_lost = true;

   va_start(ap, msg);
   err = __vk_errorv(device->instance, device,
                     VK_DEBUG_REPORT_OBJECT_TYPE_DEVICE_EXT,
                     VK_ERROR_DEVICE_LOST, file, line, msg, ap);
   va_end(ap);

   if (env_var_as_boolean("V3DVK_ABORT_ON_DEVICE_LOSS", false))
      abort();

   return err;
}

VkResult
v3dvk_device_query_status(struct v3dvk_device *device)
{
   /* This isn't likely as most of the callers of this function already check
    * for it.  However, it doesn't hurt to check and it potentially lets us
    * avoid an ioctl.
    */
   if (v3dvk_device_is_lost(device))
      return VK_ERROR_DEVICE_LOST;

   uint32_t active, pending;
   int ret = v3dvk_gem_gpu_get_reset_stats(device, &active, &pending);
   if (ret == -1) {
      /* We don't know the real error. */
      return v3dvk_device_set_lost(device, "get_reset_stats failed: %m");
   }

   if (active) {
      return v3dvk_device_set_lost(device, "GPU hung on one of our command buffers");
   } else if (pending) {
      return v3dvk_device_set_lost(device, "GPU hung with commands in-flight");
   }

   return VK_SUCCESS;
}

static void
v3dvk_device_init_dispatch(struct v3dvk_device *device)
{
   for (unsigned i = 0; i < ARRAY_SIZE(device->dispatch.entrypoints); i++) {
      /* Vulkan requires that entrypoints for extensions which have not been
       * enabled must not be advertised.
       */
      if (!v3dvk_device_entrypoint_is_enabled(i, device->instance->app_info.api_version,
                                              &device->instance->enabled_extensions,
                                              &device->enabled_extensions)) {
         device->dispatch.entrypoints[i] = NULL;
      } else {
         device->dispatch.entrypoints[i] =
            v3dvk_device_dispatch_table.entrypoints[i];
      }
   }
}

VkResult v3dvk_CreateDevice(
    VkPhysicalDevice                            physicalDevice,
    const VkDeviceCreateInfo*                   pCreateInfo,
    const VkAllocationCallbacks*                pAllocator,
    VkDevice*                                   pDevice)
{
   V3DVK_FROM_HANDLE(v3dvk_physical_device, physical_device, physicalDevice);
   VkResult result;
   struct v3dvk_device *device;

   assert(pCreateInfo->sType == VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO);

   struct v3dvk_device_extension_table enabled_extensions = { };
   for (uint32_t i = 0; i < pCreateInfo->enabledExtensionCount; i++) {
      int idx;
      for (idx = 0; idx < V3DVK_DEVICE_EXTENSION_COUNT; idx++) {
         if (strcmp(pCreateInfo->ppEnabledExtensionNames[i],
                    v3dvk_device_extensions[idx].extensionName) == 0)
            break;
      }

      if (idx >= V3DVK_DEVICE_EXTENSION_COUNT)
         return vk_error(VK_ERROR_EXTENSION_NOT_PRESENT);

      if (!physical_device->supported_extensions.extensions[idx])
         return vk_error(VK_ERROR_EXTENSION_NOT_PRESENT);

      enabled_extensions.extensions[idx] = true;
   }

   /* Check enabled features */
   if (pCreateInfo->pEnabledFeatures) {
      VkPhysicalDeviceFeatures supported_features;
      v3dvk_GetPhysicalDeviceFeatures(physicalDevice, &supported_features);
      VkBool32 *supported_feature = (VkBool32 *)&supported_features;
      VkBool32 *enabled_feature = (VkBool32 *)pCreateInfo->pEnabledFeatures;
      unsigned num_features = sizeof(VkPhysicalDeviceFeatures) / sizeof(VkBool32);
      for (uint32_t i = 0; i < num_features; i++) {
         if (enabled_feature[i] && !supported_feature[i])
            return vk_error(VK_ERROR_FEATURE_NOT_PRESENT);
      }
   }

   /* Check requested queues and fail if we are requested to create any
    * queues with flags we don't support.
    */
   assert(pCreateInfo->queueCreateInfoCount > 0);
   for (uint32_t i = 0; i < pCreateInfo->queueCreateInfoCount; i++) {
      if (pCreateInfo->pQueueCreateInfos[i].flags != 0)
         return vk_error(VK_ERROR_INITIALIZATION_FAILED);
   }

   /* Check if client specified queue priority. */
   const VkDeviceQueueGlobalPriorityCreateInfoEXT *queue_priority =
      vk_find_struct_const(pCreateInfo->pQueueCreateInfos[0].pNext,
                           DEVICE_QUEUE_GLOBAL_PRIORITY_CREATE_INFO_EXT);

   VkQueueGlobalPriorityEXT priority =
      queue_priority ? queue_priority->globalPriority :
         VK_QUEUE_GLOBAL_PRIORITY_MEDIUM_EXT;

   device = vk_alloc2(&physical_device->instance->alloc, pAllocator,
                       sizeof(*device), 8,
                       VK_SYSTEM_ALLOCATION_SCOPE_DEVICE);
   if (!device)
      return vk_error(VK_ERROR_OUT_OF_HOST_MEMORY);

#if 0
   if (INTEL_DEBUG & DEBUG_BATCH) {
      const unsigned decode_flags =
         GEN_BATCH_DECODE_FULL |
         ((INTEL_DEBUG & DEBUG_COLOR) ? GEN_BATCH_DECODE_IN_COLOR : 0) |
         GEN_BATCH_DECODE_OFFSETS |
         GEN_BATCH_DECODE_FLOATS;

      gen_batch_decode_ctx_init(&device->decoder_ctx,
                                &physical_device->info,
                                stderr, decode_flags, NULL,
                                decode_get_bo, NULL, device);
   }
#endif

   device->_loader_data.loaderMagic = ICD_LOADER_MAGIC;
   device->instance = physical_device->instance;
   device->_lost = false;

   if (pAllocator)
      device->alloc = *pAllocator;
   else
      device->alloc = physical_device->instance->alloc;

   /* XXX(chadv): Can we dup() physicalDevice->fd here? */
   device->fd = open(physical_device->path, O_RDWR | O_CLOEXEC);
   if (device->fd == -1) {
      result = vk_error(VK_ERROR_INITIALIZATION_FAILED);
      goto fail_device;
   }
#if 0
   device->context_id = anv_gem_create_context(device);
   if (device->context_id == -1) {
      result = vk_error(VK_ERROR_INITIALIZATION_FAILED);
      goto fail_fd;
   }

   if (physical_device->use_softpin) {
      if (pthread_mutex_init(&device->vma_mutex, NULL) != 0) {
         result = vk_error(VK_ERROR_INITIALIZATION_FAILED);
         goto fail_fd;
      }

      /* keep the page with address zero out of the allocator */
      struct anv_memory_heap *low_heap =
         &physical_device->memory.heaps[physical_device->memory.heap_count - 1];
      util_vma_heap_init(&device->vma_lo, low_heap->vma_start, low_heap->vma_size);
      device->vma_lo_available = low_heap->size;

      struct anv_memory_heap *high_heap =
         &physical_device->memory.heaps[0];
      util_vma_heap_init(&device->vma_hi, high_heap->vma_start, high_heap->vma_size);
      device->vma_hi_available = physical_device->memory.heap_count == 1 ? 0 :
         high_heap->size;
   }

   list_inithead(&device->memory_objects);

   /* As per spec, the driver implementation may deny requests to acquire
    * a priority above the default priority (MEDIUM) if the caller does not
    * have sufficient privileges. In this scenario VK_ERROR_NOT_PERMITTED_EXT
    * is returned.
    */
   if (physical_device->has_context_priority) {
      int err = anv_gem_set_context_param(device->fd, device->context_id,
                                          I915_CONTEXT_PARAM_PRIORITY,
                                          vk_priority_to_gen(priority));
      if (err != 0 && priority > VK_QUEUE_GLOBAL_PRIORITY_MEDIUM_EXT) {
         result = vk_error(VK_ERROR_NOT_PERMITTED_EXT);
         goto fail_fd;
      }
   }
#endif
   device->info = physical_device->info;
#if 0
   /* On Broadwell and later, we can use batch chaining to more efficiently
    * implement growing command buffers.  Prior to Haswell, the kernel
    * command parser gets in the way and we have to fall back to growing
    * the batch.
    */
   device->can_chain_batches = device->info.gen >= 8;
   device->robust_buffer_access = pCreateInfo->pEnabledFeatures &&
      pCreateInfo->pEnabledFeatures->robustBufferAccess;
#endif
   device->enabled_extensions = enabled_extensions;

   v3dvk_device_init_dispatch(device);

   if (pthread_mutex_init(&device->mutex, NULL) != 0) {
      result = vk_error(VK_ERROR_INITIALIZATION_FAILED);
      goto fail_context_id;
   }

   pthread_condattr_t condattr;
   if (pthread_condattr_init(&condattr) != 0) {
      result = vk_error(VK_ERROR_INITIALIZATION_FAILED);
      goto fail_mutex;
   }
   if (pthread_condattr_setclock(&condattr, CLOCK_MONOTONIC) != 0) {
      pthread_condattr_destroy(&condattr);
      result = vk_error(VK_ERROR_INITIALIZATION_FAILED);
      goto fail_mutex;
   }
   if (pthread_cond_init(&device->queue_submit, &condattr) != 0) {
      pthread_condattr_destroy(&condattr);
      result = vk_error(VK_ERROR_INITIALIZATION_FAILED);
      goto fail_mutex;
   }
   pthread_condattr_destroy(&condattr);

#if 0
   uint64_t bo_flags =
      (physical_device->supports_48bit_addresses ? EXEC_OBJECT_SUPPORTS_48B_ADDRESS : 0) |
      (physical_device->has_exec_async ? EXEC_OBJECT_ASYNC : 0) |
      (physical_device->has_exec_capture ? EXEC_OBJECT_CAPTURE : 0) |
      (physical_device->use_softpin ? EXEC_OBJECT_PINNED : 0);

   anv_bo_pool_init(&device->batch_bo_pool, device, bo_flags);

   result = anv_bo_cache_init(&device->bo_cache);
   if (result != VK_SUCCESS)
      goto fail_batch_bo_pool;

   if (!physical_device->use_softpin)
      bo_flags &= ~EXEC_OBJECT_SUPPORTS_48B_ADDRESS;

   result = anv_state_pool_init(&device->dynamic_state_pool, device,
                                DYNAMIC_STATE_POOL_MIN_ADDRESS,
                                16384,
                                bo_flags);
   if (result != VK_SUCCESS)
      goto fail_bo_cache;

   result = anv_state_pool_init(&device->instruction_state_pool, device,
                                INSTRUCTION_STATE_POOL_MIN_ADDRESS,
                                16384,
                                bo_flags);
   if (result != VK_SUCCESS)
      goto fail_dynamic_state_pool;

   result = anv_state_pool_init(&device->surface_state_pool, device,
                                SURFACE_STATE_POOL_MIN_ADDRESS,
                                4096,
                                bo_flags);
   if (result != VK_SUCCESS)
      goto fail_instruction_state_pool;

   if (physical_device->use_softpin) {
      result = anv_state_pool_init(&device->binding_table_pool, device,
                                   BINDING_TABLE_POOL_MIN_ADDRESS,
                                   4096,
                                   bo_flags);
      if (result != VK_SUCCESS)
         goto fail_surface_state_pool;
   }

   result = anv_bo_init_new(&device->workaround_bo, device, 4096);
   if (result != VK_SUCCESS)
      goto fail_binding_table_pool;

   if (physical_device->use_softpin)
      device->workaround_bo.flags |= EXEC_OBJECT_PINNED;

   if (!anv_vma_alloc(device, &device->workaround_bo))
      goto fail_workaround_bo;

   anv_device_init_trivial_batch(device);

   if (device->info.gen >= 10)
      anv_device_init_hiz_clear_value_bo(device);

   anv_scratch_pool_init(device, &device->scratch_pool);
#endif
   for (unsigned i = 0; i < pCreateInfo->queueCreateInfoCount; i++) {
      const VkDeviceQueueCreateInfo *queue_create =
         &pCreateInfo->pQueueCreateInfos[i];
      uint32_t qfi = queue_create->queueFamilyIndex;
      device->queues[qfi] = vk_alloc(
         &device->alloc, queue_create->queueCount * sizeof(struct v3dvk_queue),
         8, VK_SYSTEM_ALLOCATION_SCOPE_DEVICE);
      if (!device->queues[qfi]) {
         result = VK_ERROR_OUT_OF_HOST_MEMORY;
         goto fail_queues;
      }

      memset(device->queues[qfi], 0,
             queue_create->queueCount * sizeof(struct v3dvk_queue));

      device->queue_count[qfi] = queue_create->queueCount;

      for (unsigned q = 0; q < queue_create->queueCount; q++) {
         v3dvk_queue_init(device, &device->queues[qfi][q]);
      }
   }
#if 0
   switch (device->info.gen) {
   case 7:
      if (!device->info.is_haswell)
         result = gen7_init_device_state(device);
      else
         result = gen75_init_device_state(device);
      break;
   case 8:
      result = gen8_init_device_state(device);
      break;
   case 9:
      result = gen9_init_device_state(device);
      break;
   case 10:
      result = gen10_init_device_state(device);
      break;
   case 11:
      result = gen11_init_device_state(device);
      break;
   default:
      /* Shouldn't get here as we don't create physical devices for any other
       * gens. */
      unreachable("unhandled gen");
   }
#endif
   if (result != VK_SUCCESS)
      goto fail_workaround_bo;
#if 0
   anv_pipeline_cache_init(&device->default_pipeline_cache, device, true);

   anv_device_init_blorp(device);

   anv_device_init_border_colors(device);
#endif
   *pDevice = v3dvk_device_to_handle(device);

   return VK_SUCCESS;
 fail_workaround_bo:
 fail_queues:
   for (unsigned i = 0; i < pCreateInfo->queueCreateInfoCount; i++) {
      const VkDeviceQueueCreateInfo *queue_create =
         &pCreateInfo->pQueueCreateInfos[i];
      uint32_t qfi = queue_create->queueFamilyIndex;
      device->queue_count[qfi] = queue_create->queueCount;
      for (unsigned q = 0; q < queue_create->queueCount; q++) {
         v3dvk_queue_finish(&device->queues[qfi][q]);
      }
      vk_free(&device->alloc, device->queues[qfi]);
   }
#if 0
   anv_scratch_pool_finish(device, &device->scratch_pool);
   anv_gem_munmap(device->workaround_bo.map, device->workaround_bo.size);
   anv_gem_close(device, device->workaround_bo.gem_handle);
#endif
 fail_binding_table_pool:
#if 0
   if (physical_device->use_softpin)
      anv_state_pool_finish(&device->binding_table_pool);
#endif
 fail_surface_state_pool:
#if 0
   anv_state_pool_finish(&device->surface_state_pool);
#endif
 fail_instruction_state_pool:
#if 0
   anv_state_pool_finish(&device->instruction_state_pool);
#endif
 fail_dynamic_state_pool:
#if 0
   anv_state_pool_finish(&device->dynamic_state_pool);
#endif
 fail_bo_cache:
#if 0
   anv_bo_cache_finish(&device->bo_cache);
#endif
 fail_batch_bo_pool:
#if 0
   anv_bo_pool_finish(&device->batch_bo_pool);
#endif
   pthread_cond_destroy(&device->queue_submit);
 fail_mutex:
   pthread_mutex_destroy(&device->mutex);
 fail_context_id:
#if 0
   anv_gem_destroy_context(device, device->context_id);
#endif
 fail_fd:
   close(device->fd);
 fail_device:
   vk_free(&device->alloc, device);

   return result;
}

void v3dvk_DestroyDevice(
    VkDevice                                    _device,
    const VkAllocationCallbacks*                pAllocator)
{
   V3DVK_FROM_HANDLE(v3dvk_device, device, _device);
   struct v3dvk_physical_device *physical_device;

   if (!device)
      return;

   physical_device = &device->instance->physicalDevice;
#if 0
   anv_device_finish_blorp(device);

   anv_pipeline_cache_finish(&device->default_pipeline_cache);
#endif

   for (unsigned i = 0; i < V3DVK_MAX_QUEUE_FAMILIES; i++) {
      for (unsigned q = 0; q < device->queue_count[i]; q++)
         v3dvk_queue_finish(&device->queues[i][q]);
      if (device->queue_count[i])
         vk_free(&device->alloc, device->queues[i]);
   }

#ifdef HAVE_VALGRIND
   /* We only need to free these to prevent valgrind errors.  The backing
    * BO will go away in a couple of lines so we don't actually leak.
    */
   anv_state_pool_free(&device->dynamic_state_pool, device->border_colors);
   anv_state_pool_free(&device->dynamic_state_pool, device->slice_hash);
#endif
#if 0
   anv_scratch_pool_finish(device, &device->scratch_pool);

   anv_gem_munmap(device->workaround_bo.map, device->workaround_bo.size);
   anv_vma_free(device, &device->workaround_bo);
   anv_gem_close(device, device->workaround_bo.gem_handle);

   anv_vma_free(device, &device->trivial_batch_bo);
   anv_gem_close(device, device->trivial_batch_bo.gem_handle);
   if (device->info.gen >= 10)
      anv_gem_close(device, device->hiz_clear_bo.gem_handle);

   if (physical_device->use_softpin)
      anv_state_pool_finish(&device->binding_table_pool);
   anv_state_pool_finish(&device->surface_state_pool);
   anv_state_pool_finish(&device->instruction_state_pool);
   anv_state_pool_finish(&device->dynamic_state_pool);

   anv_bo_cache_finish(&device->bo_cache);

   anv_bo_pool_finish(&device->batch_bo_pool);
#endif
   pthread_cond_destroy(&device->queue_submit);
   pthread_mutex_destroy(&device->mutex);
#if 0
   anv_gem_destroy_context(device, device->context_id);

   if (INTEL_DEBUG & DEBUG_BATCH)
      gen_batch_decode_ctx_finish(&device->decoder_ctx);
#endif
   close(device->fd);

   vk_free(&device->alloc, device);
}

void
v3dvk_GetDeviceQueue(VkDevice _device,
                  uint32_t queueFamilyIndex,
                  uint32_t queueIndex,
                  VkQueue *pQueue)
{
   const VkDeviceQueueInfo2 info =
      (VkDeviceQueueInfo2) { .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_INFO_2,
                             .queueFamilyIndex = queueFamilyIndex,
                             .queueIndex = queueIndex };

   v3dvk_GetDeviceQueue2(_device, &info, pQueue);
}

void
v3dvk_GetDeviceQueue2(VkDevice _device,
                   const VkDeviceQueueInfo2 *pQueueInfo,
                   VkQueue *pQueue)
{
   V3DVK_FROM_HANDLE(v3dvk_device, device, _device);
   struct v3dvk_queue *queue;

   queue =
      &device->queues[pQueueInfo->queueFamilyIndex][pQueueInfo->queueIndex];
   if (pQueueInfo->flags != queue->flags) {
      /* From the Vulkan 1.1.70 spec:
       *
       * "The queue returned by vkGetDeviceQueue2 must have the same
       * flags value from this structure as that used at device
       * creation time in a VkDeviceQueueCreateInfo instance. If no
       * matching flags were specified at device creation time then
       * pQueue will return VK_NULL_HANDLE."
       */
      *pQueue = VK_NULL_HANDLE;
      return;
   }

   *pQueue = v3dvk_queue_to_handle(queue);
}

VkResult v3dvk_DeviceWaitIdle(
    VkDevice                                    _device)
{
   V3DVK_FROM_HANDLE(v3dvk_device, device, _device);
   if (v3dvk_device_is_lost(device))
      return VK_ERROR_DEVICE_LOST;

   for (unsigned i = 0; i < V3DVK_MAX_QUEUE_FAMILIES; i++) {
      for (unsigned q = 0; q < device->queue_count[i]; q++) {
         v3dvk_QueueWaitIdle(v3dvk_queue_to_handle(&device->queues[i][q]));
      }
   }
   return VK_SUCCESS;
}

VkResult
v3dvk_CreateBuffer(VkDevice _device,
                const VkBufferCreateInfo *pCreateInfo,
                const VkAllocationCallbacks *pAllocator,
                VkBuffer *pBuffer)
{
   V3DVK_FROM_HANDLE(v3dvk_device, device, _device);
   struct v3dvk_buffer *buffer;

   assert(pCreateInfo->sType == VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO);

   buffer = vk_alloc2(&device->alloc, pAllocator, sizeof(*buffer), 8,
                      VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
   if (buffer == NULL)
      return v3dvk_error(device->instance, VK_ERROR_OUT_OF_HOST_MEMORY);

   buffer->size = pCreateInfo->size;
   buffer->usage = pCreateInfo->usage;
#if 0
   buffer->flags = pCreateInfo->flags;
#endif
   *pBuffer = v3dvk_buffer_to_handle(buffer);

   return VK_SUCCESS;
}

void
v3dvk_DestroyBuffer(VkDevice _device,
                 VkBuffer _buffer,
                 const VkAllocationCallbacks *pAllocator)
{
   V3DVK_FROM_HANDLE(v3dvk_device, device, _device);
   V3DVK_FROM_HANDLE(v3dvk_buffer, buffer, _buffer);

   if (!buffer)
      return;

   vk_free2(&device->alloc, pAllocator, buffer);
}
