
#ifndef V3DVK_DESCRIPTOR_SET_H
#define V3DVK_DESCRIPTOR_SET_H

#include <stdint.h>
#include "v3dvk_constants.h"

struct v3dvk_pipeline_layout
{
   struct
   {
      struct v3dvk_descriptor_set_layout *layout;
#if 0
      uint32_t size;
      uint32_t dynamic_offset_start;
#endif
   } set[MAX_SETS];

   uint32_t num_sets;
#if 0
   uint32_t push_constant_size;
   uint32_t dynamic_offset_count;

   unsigned char sha1[20];
#endif
};

#endif
