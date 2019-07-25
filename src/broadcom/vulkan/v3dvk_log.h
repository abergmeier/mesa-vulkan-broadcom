
#ifndef V3DVK_LOG_H
#define V3DVK_LOG_H

#define v3dvk_printflike(a, b) __attribute__((__format__(__printf__, a, b)))

void __v3dvk_perf_warn(struct v3dvk_instance *instance, const void *object,
                       VkDebugReportObjectTypeEXT type, const char *file,
                       int line, const char *format, ...)
   v3dvk_printflike(6, 7);

void v3dvk_loge(const char *format, ...) v3dvk_printflike(1, 2);
void v3dvk_loge_v(const char *format, va_list va);

#endif
