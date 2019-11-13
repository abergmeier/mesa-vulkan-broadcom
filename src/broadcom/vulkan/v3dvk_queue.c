
#include <assert.h>
#include <vulkan/vulkan.h>
#include "common.h"
#include "device.h"
#include "v3dvk_cmd_buffer.h"
#include "v3dvk_queue.h"

VkResult v3dvk_QueueSubmit(
    VkQueue                                     _queue,
    uint32_t                                    submitCount,
    const VkSubmitInfo*                         pSubmits,
    VkFence                                     fence)
{
   V3DVK_FROM_HANDLE(v3dvk_queue, queue, _queue);
   struct v3dvk_device *device = queue->device;

   /* Query for device status prior to submitting.  Technically, we don't need
    * to do this.  However, if we have a client that's submitting piles of
    * garbage, we would rather break as early as possible to keep the GPU
    * hanging contained.  If we don't check here, we'll either be waiting for
    * the kernel to kick us or we'll have to wait until the client waits on a
    * fence before we actually know whether or not we've hung.
    */
   VkResult result = v3dvk_device_query_status(device);
   if (result != VK_SUCCESS)
      return result;

#if 0
   /* We lock around QueueSubmit for three main reasons:
    *
    *  1) When a block pool is resized, we create a new gem handle with a
    *     different size and, in the case of surface states, possibly a
    *     different center offset but we re-use the same anv_bo struct when
    *     we do so.  If this happens in the middle of setting up an execbuf,
    *     we could end up with our list of BOs out of sync with our list of
    *     gem handles.
    *
    *  2) The algorithm we use for building the list of unique buffers isn't
    *     thread-safe.  While the client is supposed to syncronize around
    *     QueueSubmit, this would be extremely difficult to debug if it ever
    *     came up in the wild due to a broken app.  It's better to play it
    *     safe and just lock around QueueSubmit.
    *
    *  3)  The anv_cmd_buffer_execbuf function may perform relocations in
    *      userspace.  Due to the fact that the surface state buffer is shared
    *      between batches, we can't afford to have that happen from multiple
    *      threads at the same time.  Even though the user is supposed to
    *      ensure this doesn't happen, we play it safe as in (2) above.
    *
    * Since the only other things that ever take the device lock such as block
    * pool resize only rarely happen, this will almost never be contended so
    * taking a lock isn't really an expensive operation in this case.
    */
   pthread_mutex_lock(&device->mutex);
#endif
   if (fence && submitCount == 0) {
      /* If we don't have any command buffers, we need to submit a dummy
       * batch to give GEM something to wait on.  We could, potentially,
       * come up with something more efficient but this shouldn't be a
       * common case.
       */
      result = v3dvk_cmd_buffer_execbuf(device, NULL, NULL, 0, NULL, 0, fence);
      goto out;
   }

   for (uint32_t i = 0; i < submitCount; i++) {
      /* Fence for this submit.  NULL for all but the last one */
      VkFence submit_fence = (i == submitCount - 1) ? fence : VK_NULL_HANDLE;

      if (pSubmits[i].commandBufferCount == 0) {
         /* If we don't have any command buffers, we need to submit a dummy
          * batch to give GEM something to wait on.  We could, potentially,
          * come up with something more efficient but this shouldn't be a
          * common case.
          */
         result = v3dvk_cmd_buffer_execbuf(device, NULL,
                                         pSubmits[i].pWaitSemaphores,
                                         pSubmits[i].waitSemaphoreCount,
                                         pSubmits[i].pSignalSemaphores,
                                         pSubmits[i].signalSemaphoreCount,
                                         submit_fence);
         if (result != VK_SUCCESS)
            goto out;

         continue;
      }

      for (uint32_t j = 0; j < pSubmits[i].commandBufferCount; j++) {
         V3DVK_FROM_HANDLE(v3dvk_cmd_buffer, cmd_buffer,
                         pSubmits[i].pCommandBuffers[j]);
         assert(cmd_buffer->level == VK_COMMAND_BUFFER_LEVEL_PRIMARY);

         /* Fence for this execbuf.  NULL for all but the last one */
         VkFence execbuf_fence =
            (j == pSubmits[i].commandBufferCount - 1) ?
            submit_fence : VK_NULL_HANDLE;

         const VkSemaphore *in_semaphores = NULL, *out_semaphores = NULL;
         uint32_t num_in_semaphores = 0, num_out_semaphores = 0;
         if (j == 0) {
            /* Only the first batch gets the in semaphores */
            in_semaphores = pSubmits[i].pWaitSemaphores;
            num_in_semaphores = pSubmits[i].waitSemaphoreCount;
         }

         if (j == pSubmits[i].commandBufferCount - 1) {
            /* Only the last batch gets the out semaphores */
            out_semaphores = pSubmits[i].pSignalSemaphores;
            num_out_semaphores = pSubmits[i].signalSemaphoreCount;
         }

         result = v3dvk_cmd_buffer_execbuf(device, cmd_buffer,
                                         in_semaphores, num_in_semaphores,
                                         out_semaphores, num_out_semaphores,
                                         execbuf_fence);
         if (result != VK_SUCCESS)
            goto out;
      }
   }
#if 0
   pthread_cond_broadcast(&device->queue_submit);
#endif
out:
   if (result != VK_SUCCESS) {
      /* In the case that something has gone wrong we may end up with an
       * inconsistent state from which it may not be trivial to recover.
       * For example, we might have computed address relocations and
       * any future attempt to re-submit this job will need to know about
       * this and avoid computing relocation addresses again.
       *
       * To avoid this sort of issues, we assume that if something was
       * wrong during submission we must already be in a really bad situation
       * anyway (such us being out of memory) and return
       * VK_ERROR_DEVICE_LOST to ensure that clients do not attempt to
       * submit the same job again to this device.
       */
      result = v3dvk_device_set_lost(device, "vkQueueSubmit() failed");
   }
#if 0
   pthread_mutex_unlock(&device->mutex);
#endif
   return result;
}

VkResult v3dvk_QueueWaitIdle(
    VkQueue                                     _queue)
{
   V3DVK_FROM_HANDLE(v3dvk_queue, queue, _queue);

   v3dvk_fence_wait_idle(&queue->submit_fence);

   return VK_SUCCESS;
}

void
v3dvk_queue_init(struct v3dvk_device *device, struct v3dvk_queue *queue)
{
   queue->_loader_data.loaderMagic = ICD_LOADER_MAGIC;
   queue->device = device;
   queue->flags = 0;

   v3dvk_fence_init(&queue->submit_fence, false);
}

void
v3dvk_queue_finish(struct v3dvk_queue *queue)
{
   v3dvk_fence_finish(&queue->submit_fence);
}
