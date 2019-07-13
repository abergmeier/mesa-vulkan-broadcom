/*
 * Copyright © 2019 Andreas Bergmeier
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

#include <stdio.h>

#include "gen_device_info.h"

bool
gen_get_device_info(int devid, struct gen_device_info *devinfo)
{
   switch (devid) {
#undef CHIPSET
#define CHIPSET(id, family, name) \
      case id: *devinfo = gen_device_info_##family; break;
   default:
      fprintf(stderr, "Driver does not support the 0x%x PCI ID.\n", devid);
      return false;
   }

   return true;
}

const char *
gen_get_device_name(int devid)
{
   switch (devid) {
#undef CHIPSET
#define CHIPSET(id, family, name) case id: return name;
   default:
      return NULL;
   }
}
