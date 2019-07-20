
#ifndef V3DVK_PIPELINE_CACHE_H
#define V3DVK_PIPELINE_CACHE_H

#include <vulkan/vulkan.h>

struct v3dvk_pipeline_cache
{
   struct v3dvk_device *device;
#if 0
   pthread_mutex_t mutex;

   uint32_t total_size;
   uint32_t table_size;
   uint32_t kernel_count;
   struct cache_entry **hash_table;
   bool modified;
#endif
   VkAllocationCallbacks alloc;
};

#endif
