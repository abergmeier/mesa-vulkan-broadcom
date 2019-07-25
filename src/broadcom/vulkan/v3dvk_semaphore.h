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

#ifndef V3DVK_SEMAPHORE_H
#define V3DVK_SEMAPHORE_H

enum v3dvk_semaphore_type {
   V3DVK_SEMAPHORE_TYPE_NONE = 0,
   V3DVK_SEMAPHORE_TYPE_DUMMY,
#if 0
   V3DVK_SEMAPHORE_TYPE_BO,
#endif
   V3DVK_SEMAPHORE_TYPE_SYNC_FILE,
#if 0
   V3DVK_SEMAPHORE_TYPE_DRM_SYNCOBJ,
#endif
};

struct v3dvk_semaphore_impl {
   enum v3dvk_semaphore_type type;

   union {
#if 0
      /* A BO representing this semaphore when type == ANV_SEMAPHORE_TYPE_BO.
       * This BO will be added to the object list on any execbuf2 calls for
       * which this semaphore is used as a wait or signal fence.  When used as
       * a signal fence, the EXEC_OBJECT_WRITE flag will be set.
       */
      struct anv_bo *bo;
#endif
      /* The sync file descriptor when type == ANV_SEMAPHORE_TYPE_SYNC_FILE.
       * If the semaphore is in the unsignaled state due to either just being
       * created or because it has been used for a wait, fd will be -1.
       */
      int fd;
#if 0
      /* Sync object handle when type == ANV_SEMAPHORE_TYPE_DRM_SYNCOBJ.
       * Unlike GEM BOs, DRM sync objects aren't deduplicated by the kernel on
       * import so we don't need to bother with a userspace cache.
       */
      uint32_t syncobj;
#endif
   };
};

struct v3dvk_semaphore {
   // TODO: Check whether we have to have both permanent and temporary at the same time
   /* Permanent semaphore state.  Every semaphore has some form of permanent
    * state (type != ANV_SEMAPHORE_TYPE_NONE).  This may be a BO to fence on
    * (for cross-process semaphores0 or it could just be a dummy for use
    * internally.
    */
   struct v3dvk_semaphore_impl permanent;

   /* Temporary semaphore state.  A semaphore *may* have temporary state.
    * That state is added to the semaphore by an import operation and is reset
    * back to V3DVK_SEMAPHORE_TYPE_NONE when the semaphore is waited on.  A
    * semaphore with temporary state cannot be signaled because the semaphore
    * must already be signaled before the temporary state can be exported from
    * the semaphore in the other process and imported here.
    */
   struct v3dvk_semaphore_impl temporary;
};

#endif
