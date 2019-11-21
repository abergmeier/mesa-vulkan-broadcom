#ifndef V3DVK_COMMON_H
#define V3DVK_COMMON_H

#include <stdarg.h>

#include <vulkan/vulkan.h>

struct v3dvk_instance;

/* Whenever we generate an error, pass it through this function. Useful for
 * debugging, where we can break on it. Only call at error site, not when
 * propagating errors. Might be useful to plug in a stack trace here.
 */

VkResult __vk_errorv(struct v3dvk_instance *instance, const void *object,
                     VkDebugReportObjectTypeEXT type, VkResult error,
                     const char *file, int line, const char *format,
                     va_list args);

VkResult __vk_errorf(struct v3dvk_instance *instance, const void *object,
                     VkDebugReportObjectTypeEXT type, VkResult error,
                     const char *file, int line, const char *format, ...);

#ifdef DEBUG
#define vk_error(error) __vk_errorf(NULL, NULL,\
                                    VK_DEBUG_REPORT_OBJECT_TYPE_UNKNOWN_EXT,\
                                    error, __FILE__, __LINE__, NULL)
#define vk_errorv(instance, obj, error, format, args)\
    __vk_errorv(instance, obj, REPORT_OBJECT_TYPE(obj), error,\
                __FILE__, __LINE__, format, args)
#define vk_errorf(instance, obj, error, format, ...)\
    __vk_errorf(instance, obj, REPORT_OBJECT_TYPE(obj), error,\
                __FILE__, __LINE__, format, ## __VA_ARGS__)
#else
#define vk_error(error) error
#define vk_errorf(instance, obj, error, format, ...) error
#endif

#define V3DVK_DEFINE_HANDLE_CASTS(__v3dvk_type, __VkType)                  \
                                                                           \
   static inline struct __v3dvk_type *                                     \
   __v3dvk_type ## _from_handle(__VkType _handle)                          \
   {                                                                       \
      return (struct __v3dvk_type *) _handle;                              \
   }                                                                       \
                                                                           \
   static inline __VkType                                                  \
   __v3dvk_type ## _to_handle(struct __v3dvk_type *_obj)                   \
   {                                                                       \
      return (__VkType) _obj;                                              \
   }

#define V3DVK_DEFINE_NONDISP_HANDLE_CASTS(__v3dvk_type, __VkType)          \
                                                                           \
   static inline struct __v3dvk_type *                                     \
   __v3dvk_type ## _from_handle(__VkType _handle)                          \
   {                                                                       \
      return (struct __v3dvk_type *)(uintptr_t) _handle;                   \
   }                                                                       \
                                                                           \
   static inline __VkType                                                  \
   __v3dvk_type ## _to_handle(struct __v3dvk_type *_obj)                   \
   {                                                                       \
      return (__VkType)(uintptr_t) _obj;                                   \
   }

#define V3DVK_FROM_HANDLE(__v3dvk_type, __name, __handle) \
   struct __v3dvk_type *__name = __v3dvk_type ## _from_handle(__handle)

V3DVK_DEFINE_HANDLE_CASTS(v3dvk_cmd_buffer, VkCommandBuffer)
V3DVK_DEFINE_HANDLE_CASTS(v3dvk_device, VkDevice)
V3DVK_DEFINE_HANDLE_CASTS(v3dvk_instance, VkInstance)
V3DVK_DEFINE_HANDLE_CASTS(v3dvk_physical_device, VkPhysicalDevice)
V3DVK_DEFINE_HANDLE_CASTS(v3dvk_queue, VkQueue)

V3DVK_DEFINE_NONDISP_HANDLE_CASTS(v3dvk_buffer, VkBuffer)
V3DVK_DEFINE_NONDISP_HANDLE_CASTS(v3dvk_cmd_pool, VkCommandPool)
V3DVK_DEFINE_NONDISP_HANDLE_CASTS(v3dvk_device_memory, VkDeviceMemory)
V3DVK_DEFINE_NONDISP_HANDLE_CASTS(v3dvk_event, VkEvent)
V3DVK_DEFINE_NONDISP_HANDLE_CASTS(v3dvk_fence, VkFence)
V3DVK_DEFINE_NONDISP_HANDLE_CASTS(v3dvk_framebuffer, VkFramebuffer)
V3DVK_DEFINE_NONDISP_HANDLE_CASTS(v3dvk_image, VkImage)
V3DVK_DEFINE_NONDISP_HANDLE_CASTS(v3dvk_image_view, VkImageView)
V3DVK_DEFINE_NONDISP_HANDLE_CASTS(v3dvk_query_pool, VkQueryPool)
V3DVK_DEFINE_NONDISP_HANDLE_CASTS(v3dvk_render_pass, VkRenderPass)
V3DVK_DEFINE_NONDISP_HANDLE_CASTS(v3dvk_sampler, VkSampler)
V3DVK_DEFINE_NONDISP_HANDLE_CASTS(v3dvk_semaphore, VkSemaphore)

#endif // V3DVK_COMMON_H
