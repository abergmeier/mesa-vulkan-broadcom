/*
 * Copyright © 2014-2018 Broadcom
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

#ifndef V3DVK_FORMAT_TABLE_H
#define V3DVK_FORMAT_TABLE_H

#include <stdbool.h>
#include <stdint.h>

#include <vulkan/vulkan.h>
#include "cle/v3d_packet_v42_pack.h"

struct v3dvk_format;

const struct v3dvk_format *
v3d42_get_format_desc(VkFormat f);

void
v3d42_get_internal_type_bpp_for_output_format(uint32_t format,
                                              uint32_t *type,
                                              uint32_t *bpp);

bool
v3d42_tfu_supports_tex_format(enum V3D42_Texture_Data_Formats format);

#endif
