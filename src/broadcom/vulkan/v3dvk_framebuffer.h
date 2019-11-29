
#ifndef V3DVK_FRAMEBUFFER_H
#define V3DVK_FRAMEBUFFER_H

#include <stdint.h>

struct v3dvk_framebuffer
{
   uint32_t width;
   uint32_t height;
   uint32_t layers;
#if 0
   uint32_t attachment_count;
   struct v3dvk_attachment_info attachments[0];
#endif
};

#endif
