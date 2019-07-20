/*
 * Copyright © 2019 Google LLC
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
#include <unistd.h>

#include <libsync.h>
#include "common.h"
#include "device.h"
#include "vk_alloc.h"
#include "v3dvk_entrypoints.h"
#include "v3dvk_error.h"
#include "v3dvk_fence.h"
#include "v3dvk_log.h"
#include "util/macros.h"

/**
 * Internally, a fence can be in one of these states.
 */
enum v3dvk_fence_state
{
   V3DVK_FENCE_STATE_RESET,
   V3DVK_FENCE_STATE_PENDING,
   V3DVK_FENCE_STATE_SIGNALED,
};

static enum v3dvk_fence_state
v3dvk_fence_get_state(const struct v3dvk_fence *fence)
{
   if (fence->signaled)
      assert(fence->fd < 0);

   if (fence->signaled)
      return V3DVK_FENCE_STATE_SIGNALED;
   else if (fence->fd >= 0)
      return V3DVK_FENCE_STATE_PENDING;
   else
      return V3DVK_FENCE_STATE_RESET;
}

static void
v3dvk_fence_set_state(struct v3dvk_fence *fence, enum v3dvk_fence_state state, int fd)
{
   if (fence->fd >= 0)
      close(fence->fd);

   switch (state) {
   case V3DVK_FENCE_STATE_RESET:
      assert(fd < 0);
      fence->signaled = false;
      fence->fd = -1;
      break;
   case V3DVK_FENCE_STATE_PENDING:
      assert(fd >= 0);
      fence->signaled = false;
      fence->fd = fd;
      break;
   case V3DVK_FENCE_STATE_SIGNALED:
      assert(fd < 0);
      fence->signaled = true;
      fence->fd = -1;
      break;
   default:
      unreachable("unknown fence state");
      break;
   }
}

void
v3dvk_fence_init(struct v3dvk_fence *fence, bool signaled)
{
   fence->signaled = signaled;
   fence->fd = -1;
}

void
v3dvk_fence_finish(struct v3dvk_fence *fence)
{
   if (fence->fd >= 0)
      close(fence->fd);
}


/**
 * Wait until a fence is idle (i.e., not pending).
 */
void
v3dvk_fence_wait_idle(struct v3dvk_fence *fence)
{
   if (fence->fd >= 0) {
      if (sync_wait(fence->fd, -1))
         v3dvk_loge("sync_wait on fence fd %d failed", fence->fd);

      v3dvk_fence_set_state(fence, V3DVK_FENCE_STATE_SIGNALED, -1);
   }
}

VkResult
v3dvk_CreateFence(VkDevice _device,
                  const VkFenceCreateInfo *pCreateInfo,
                  const VkAllocationCallbacks *pAllocator,
                  VkFence *pFence)
{
   V3DVK_FROM_HANDLE(v3dvk_device, device, _device);

   struct v3dvk_fence *fence =
      vk_alloc2(&device->alloc, pAllocator, sizeof(*fence), 8,
                VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);

   if (!fence)
      return v3dvk_error(device->instance, VK_ERROR_OUT_OF_HOST_MEMORY);

   v3dvk_fence_init(fence, pCreateInfo->flags & VK_FENCE_CREATE_SIGNALED_BIT);

   *pFence = v3dvk_fence_to_handle(fence);

   return VK_SUCCESS;
}

void
v3dvk_DestroyFence(VkDevice _device,
                   VkFence _fence,
                   const VkAllocationCallbacks *pAllocator)
{
   V3DVK_FROM_HANDLE(v3dvk_device, device, _device);
   V3DVK_FROM_HANDLE(v3dvk_fence, fence, _fence);

   if (!fence)
      return;

   v3dvk_fence_finish(fence);

   vk_free2(&device->alloc, pAllocator, fence);
}
