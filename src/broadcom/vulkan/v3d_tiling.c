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

/** @file v3d_tiling.c
 *
 * Handles information about the VC5 tiling formats, and loading and storing
 * from them.
 */

#include <assert.h>
#include <stdint.h>
#include "v3d_tiling.h"
// Implicit include needed v3d_cpu_tiling.h
#include <string.h>
#include "broadcom/common/v3d_cpu_tiling.h"

/** Return the width in pixels of a 64-byte microtile. */
uint32_t
v3d_utile_width(int cpp)
{
        switch (cpp) {
        case 1:
        case 2:
                return 8;
        case 4:
        case 8:
                return 4;
        case 16:
                return 2;
        default:
                unreachable("unknown cpp");
        }
}

/** Return the height in pixels of a 64-byte microtile. */
uint32_t
v3d_utile_height(int cpp)
{
        switch (cpp) {
        case 1:
                return 8;
        case 2:
        case 4:
                return 4;
        case 8:
        case 16:
                return 2;
        default:
                unreachable("unknown cpp");
        }
}

/**
 * Returns the byte address for a given pixel within a utile.
 *
 * Utiles are 64b blocks of pixels in raster order, with 32bpp being a 4x4
 * arrangement.
 */
static inline uint32_t
v3d_get_utile_pixel_offset(uint32_t cpp, uint32_t x, uint32_t y)
{
        uint32_t utile_w = v3d_utile_width(cpp);

        assert(x < utile_w && y < v3d_utile_height(cpp));

        return x * cpp + y * utile_w * cpp;
}
