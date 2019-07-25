
#ifndef V3DVK_GEM_H
#define V3DVK_GEM_H

#include <stdint.h>

struct v3dvk_device;

int v3dvk_gem_gpu_get_reset_stats(struct v3dvk_device *device,
                                  uint32_t *active, uint32_t *pending);

#endif
