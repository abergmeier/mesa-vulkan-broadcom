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

#include <stdarg.h>
#include <stdlib.h>
#include "util/debug.h"
#include "common.h"
#include "device.h"
#include "v3dvk_gem.h"

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

