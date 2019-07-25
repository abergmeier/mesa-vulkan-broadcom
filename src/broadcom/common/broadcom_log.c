/*
 * Copyright © 2017 Google, Inc.
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
#include <stdio.h>


#include "broadcom_log.h"


static inline const char *
level_to_str(enum broadcom_log_level l)
{
   switch (l) {
   case BROADCOM_LOG_ERROR: return "error";
   case BROADCOM_LOG_WARN: return "warning";
   case BROADCOM_LOG_INFO: return "info";
   case BROADCOM_LOG_DEBUG: return "debug";
   }

   unreachable("bad broadcom_log_level");
}

void
broadcom_log(enum broadcom_log_level level, const char *tag, const char *format, ...)
{
   va_list va;

   va_start(va, format);
   broadcom_log_v(level, tag, format, va);
   va_end(va);
}

void
broadcom_log_v(enum broadcom_log_level level, const char *tag, const char *format,
               va_list va)
{
   flockfile(stderr);
   fprintf(stderr, "%s: %s: ", tag, level_to_str(level));
   vfprintf(stderr, format, va);
   fprintf(stderr, "\n");
   funlockfile(stderr);
}
