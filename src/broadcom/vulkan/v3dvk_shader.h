
#ifndef V3DVK_SHADER_H
#define V3DVK_SHADER_H

#include <stdint.h>

struct v3dvk_shader_module
{
   unsigned char sha1[20];

   uint32_t code_size;
   const uint32_t *code[0];
};

#endif
