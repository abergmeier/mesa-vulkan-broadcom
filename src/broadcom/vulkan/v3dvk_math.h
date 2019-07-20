#ifndef V3DVK_MATH_H
#define V3DVK_MATH_H

#include <stdint.h>
#include "util/macros.h"

static inline uint32_t
v3dvk_minify(uint32_t n, uint32_t levels)
{
   if (unlikely(n == 0))
      return 0;
   else
      return MAX2(n >> levels, 1);
}

#endif
