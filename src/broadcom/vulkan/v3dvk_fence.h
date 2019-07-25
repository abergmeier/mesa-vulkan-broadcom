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

#ifndef V3DVK_FENCE_H
#define V3DVK_FENCE_H

enum v3dvk_fence_type {
   V3DVK_FENCE_TYPE_NONE = 0,
#if 0
   V3DVK_FENCE_TYPE_BO,
#endif
   V3DVK_FENCE_TYPE_SYNCOBJ,
   V3DVK_FENCE_TYPE_WSI,
};

enum anv_bo_fence_state {
   /** Indicates that this is a new (or newly reset fence) */
   V3DVK_BO_FENCE_STATE_RESET,

   /** Indicates that this fence has been submitted to the GPU but is still
    * (as far as we know) in use by the GPU.
    */
   V3DVK_BO_FENCE_STATE_SUBMITTED,

   V3DVK_BO_FENCE_STATE_SIGNALED,
};

struct v3dvk_fence_impl {
   enum v3dvk_fence_type type;

   union {
#if 0
      /** Fence implementation for BO fences
       *
       * These fences use a BO and a set of CPU-tracked state flags.  The BO
       * is added to the object list of the last execbuf call in a QueueSubmit
       * and is marked EXEC_WRITE.  The state flags track when the BO has been
       * submitted to the kernel.  We need to do this because Vulkan lets you
       * wait on a fence that has not yet been submitted and I915_GEM_BUSY
       * will say it's idle in this case.
       */
      struct {
         struct anv_bo bo;
         enum anv_bo_fence_state state;
      } bo;

      /** DRM syncobj handle for syncobj-based fences */
      uint32_t syncobj;
#endif
      /** WSI fence */
      struct wsi_fence *fence_wsi;
   };
};

struct v3dvk_fence {
   /* Permanent fence state.  Every fence has some form of permanent state
    * (type != ANV_SEMAPHORE_TYPE_NONE).  This may be a BO to fence on (for
    * cross-process fences) or it could just be a dummy for use internally.
    */
   struct v3dvk_fence_impl permanent;

   /* Temporary fence state.  A fence *may* have temporary state.  That state
    * is added to the fence by an import operation and is reset back to
    * ANV_SEMAPHORE_TYPE_NONE when the fence is reset.  A fence with temporary
    * state cannot be signaled because the fence must already be signaled
    * before the temporary state can be exported from the fence in the other
    * process and imported here.
    */
   struct v3dvk_fence_impl temporary;
};

#endif
