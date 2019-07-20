/*
 * Copyright 2017 Google
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

#ifndef BROADCOM_LOG_H
#define BROADCOM_LOG_H

#include <stdarg.h>

#include "util/macros.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef BROADCOM_LOG_TAG
#define BROADCOM_LOG_TAG "BROADCOM-MESA"
#endif

enum broadcom_log_level {
   BROADCOM_LOG_ERROR,
   BROADCOM_LOG_WARN,
   BROADCOM_LOG_INFO,
   BROADCOM_LOG_DEBUG,
};

void PRINTFLIKE(3, 4)
broadcom_log(enum broadcom_log_level, const char *tag, const char *format, ...);

void
broadcom_log_v(enum broadcom_log_level, const char *tag, const char *format,
                va_list va);

#define broadcom_loge(fmt, ...) broadcom_log(BROADCOM_LOG_ERROR, (BROADCOM_LOG_TAG), (fmt), ##__VA_ARGS__)
#define broadcom_logw(fmt, ...) broadcom_log(BROADCOM_LOG_WARN, (BROADCOM_LOG_TAG), (fmt), ##__VA_ARGS__)
#define broadcom_logi(fmt, ...) broadcom_log(BROADCOM_LOG_INFO, (BROADCOM_LOG_TAG), (fmt), ##__VA_ARGS__)
#ifdef DEBUG
#define broadcom_logd(fmt, ...) broadcom_log(BROADCOM_LOG_DEBUG, (BROADCOM_LOG_TAG), (fmt), ##__VA_ARGS__)
#else
#define broadcom_logd(fmt, ...) __broadcom_log_use_args((fmt), ##__VA_ARGS__)
#endif

#define broadcom_loge_v(fmt, va) broadcom_log_v(BROADCOM_LOG_ERROR, (BROADCOM_LOG_TAG), (fmt), (va))
#define broadcom_logw_v(fmt, va) broadcom_log_v(BROADCOM_LOG_WARN, (BROADCOM_LOG_TAG), (fmt), (va))
#define broadcom_logi_v(fmt, va) broadcom_log_v(BROADCOM_LOG_INFO, (BROADCOM_LOG_TAG), (fmt), (va))
#ifdef DEBUG
#define broadcom_logd_v(fmt, va) broadcom_log_v(BROADCOM_LOG_DEBUG, (BROADCOM_LOG_TAG), (fmt), (va))
#else
#define broadcom_logd_v(fmt, va) __broadcom_log_use_args((fmt), (va))
#endif


#ifndef DEBUG
/* Suppres -Wunused */
static inline void PRINTFLIKE(1, 2)
__broadcom_log_use_args(const char *format, ...) { }
#endif

#ifdef __cplusplus
}
#endif

#endif /* INTEL_LOG_H */
