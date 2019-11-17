/*
 * Copyright © 2014-2017 Broadcom
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

#ifndef V3DVK_BO_H
#define V3DVK_BO_H

#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include "util/list.h"
#include "device.h"

struct v3dvk_bo {
#if 0
        struct pipe_reference reference;
        struct v3d_screen *screen;
        void *map;
#endif
        const char *name;
        uint32_t handle;
        uint32_t size;

        /* Address of the BO in our page tables. */
        uint32_t offset;

        /** Entry in the linked list of buffers freed, by age. */
        struct list_head time_list;
        /** Entry in the per-page-count linked list of buffers freed (by age). */
        struct list_head size_list;
        /** Approximate second when the bo was freed. */
        time_t free_time;
        /**
         * Whether only our process has a reference to the BO (meaning that
         * it's safe to reuse it in the BO cache).
         */
        bool private;
};


VkResult
v3dvk_bo_init_new(struct v3dvk_device *dev, struct v3dvk_bo *bo, uint64_t size);
void
v3dvk_bo_finish(struct v3dvk_device *dev, struct v3dvk_bo *bo);

#endif /* V3DVK_BO_H */
