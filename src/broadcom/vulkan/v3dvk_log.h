
#ifndef V3DVK_LOG_H
#define V3DVK_LOG_H

#include <stdarg.h>
#include <vulkan/vulkan_core.h>

#define V3DVK_PRINTFLIKE(a, b) __attribute__((__format__(__printf__, a, b)))

struct v3dvk_instance;

void __v3dvk_perf_warn(struct v3dvk_instance *instance, const void *object,
                       VkDebugReportObjectTypeEXT type, const char *file,
                       int line, const char *format, ...)
   V3DVK_PRINTFLIKE(6, 7);

void
__v3dvk_finishme(const char *file, int line, const char *format, ...)
   V3DVK_PRINTFLIKE(3, 4);

void v3dvk_loge(const char *format, ...) V3DVK_PRINTFLIKE(1, 2);
void v3dvk_loge_v(const char *format, va_list va);

/**
 * Print a FINISHME message, including its source location.
 */
#define v3dvk_finishme(format, ...)                                          \
   do {                                                                      \
      static bool reported = false;                                          \
      if (!reported) {                                                       \
         __v3dvk_finishme(__FILE__, __LINE__, format, ##__VA_ARGS__);        \
         reported = true;                                                    \
      }                                                                      \
   } while (0)

#define V3DVK_STUB()                                                         \
   do {                                                                      \
      V3DVK_FINISHME("stub %s", __func__);                                   \
   } while (0)

#endif
