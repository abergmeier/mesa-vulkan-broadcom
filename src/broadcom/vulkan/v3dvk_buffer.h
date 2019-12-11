

#ifndef V3DVK_BUFFER_H
#define V3DVK_BUFFER_H

#include <assert.h>
#include <vulkan/vulkan.h>

struct v3dvk_device;

struct v3dvk_address {
   struct v3dvk_bo *bo;
   uint32_t offset;
};

struct v3dvk_buffer {
   struct v3dvk_device *                        device;
   VkDeviceSize                                 size;

   VkBufferUsageFlags                           usage;

   /* Set when bound */
   struct v3dvk_address                         address;


   struct v3dvk_bo *bo;
   VkDeviceSize bo_offset;
};

static inline uint64_t
v3dvk_buffer_get_range(struct v3dvk_buffer *buffer, uint64_t offset, uint64_t range)
{
   assert(offset <= buffer->size);
   if (range == VK_WHOLE_SIZE) {
      return buffer->size - offset;
   } else {
      assert(range + offset >= range);
      assert(range + offset <= buffer->size);
      return range;
   }
}

#endif
