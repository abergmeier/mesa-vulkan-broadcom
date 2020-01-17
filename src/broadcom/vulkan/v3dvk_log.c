#include <stdio.h>
#include "common/broadcom_log.h"
#include "v3dvk_log.h"

/** Log an error message.  */
void V3DVK_PRINTFLIKE(1, 2)
v3dvk_loge(const char *format, ...)
{
   va_list va;

   va_start(va, format);
   v3dvk_loge_v(format, va);
   va_end(va);
}

/** \see v3dvk_loge() */
void
v3dvk_loge_v(const char *format, va_list va)
{
   broadcom_loge_v(format, va);
}

/** Log an error message.  */
void V3DVK_PRINTFLIKE(1, 2) v3dvk_logi(const char *format, ...)
{
   va_list va;

   va_start(va, format);
   v3dvk_logi_v(format, va);
   va_end(va);
}

/** \see v3dvk_logi() */
void
v3dvk_logi_v(const char *format, va_list va)
{
   fprintf(stderr, "v3dvk: info: ");
   vfprintf(stderr, format, va);
   fprintf(stderr, "\n");
}

void V3DVK_PRINTFLIKE(3, 4)
   __v3dvk_finishme(const char *file, int line, const char *format, ...)
{
   va_list ap;
   char buffer[256];

   va_start(ap, format);
   vsnprintf(buffer, sizeof(buffer), format, ap);
   va_end(ap);

   fprintf(stderr, "%s:%d: FINISHME: %s\n", file, line, buffer);
}
