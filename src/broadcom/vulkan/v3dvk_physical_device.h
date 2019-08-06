#ifndef V3DVK_PHYSICAL_DEVICE_H
#define V3DVK_PHYSICAL_DEVICE_H

#include <vulkan/vk_icd.h>

#include <xf86drm.h>

#include "vulkan/wsi/wsi_common.h"
#include "common/v3d_device_info.h"

#include "v3dvk_memory.h"

struct v3dvk_physical_device {
    VK_LOADER_DATA                              _loader_data;

    struct v3dvk_instance *                     instance;
    bool                                        no_hw;
    char                                        path[20];
    const char *                                name;
    struct v3d_device_info                      info;
#if 0
    /** Amount of "GPU memory" we want to advertise
     *
     * Clearly, this value is bogus since Intel is a UMA architecture.  On
     * gen7 platforms, we are limited by GTT size unless we want to implement
     * fine-grained tracking and GTT splitting.  On Broadwell and above we are
     * practically unlimited.  However, we will never report more than 3/4 of
     * the total system ram to try and avoid running out of RAM.
     */
    bool                                        supports_48bit_addresses;
    struct brw_compiler *                       compiler;
    struct isl_device                           isl_dev;
    int                                         cmd_parser_version;
    bool                                        has_exec_async;
    bool                                        has_exec_capture;
    bool                                        has_exec_fence;
    bool                                        has_syncobj;
    bool                                        has_syncobj_wait;
    bool                                        has_context_priority;
    bool                                        use_softpin;
    bool                                        has_context_isolation;
    bool                                        has_mem_available;
    bool                                        always_use_bindless;

    /** True if we can access buffers using A64 messages */
    bool                                        has_a64_buffer_access;
    /** True if we can use bindless access for images */
    bool                                        has_bindless_images;
    /** True if we can use bindless access for samplers */
    bool                                        has_bindless_samplers;
#endif
    drmPlatformBusInfo bus_info;

    struct v3dvk_device_extension_table         supported_extensions;
#if 0
    uint32_t                                    eu_total;
    uint32_t                                    subslice_total;
#endif
    struct {
      uint32_t                                  type_count;
      struct v3dvk_memory_type                  types[VK_MAX_MEMORY_TYPES];
      uint32_t                                  heap_count;
      struct v3dvk_memory_heap                  heaps[VK_MAX_MEMORY_HEAPS];
    } memory;

    uint8_t                                     driver_build_sha1[20];
    uint8_t                                     pipeline_cache_uuid[VK_UUID_SIZE];
    uint8_t                                     driver_uuid[VK_UUID_SIZE];
    uint8_t                                     device_uuid[VK_UUID_SIZE];
#if 0

    struct disk_cache *                         disk_cache;
#endif
    struct wsi_device                           wsi_device;

    int                                         local_fd;
    int                                         master_fd;
};

VkResult
v3dvk_physical_device_init(struct v3dvk_physical_device *device,
                         struct v3dvk_instance *instance,
                         drmDevicePtr drm_device);


uint32_t v3dvk_physical_device_api_version(struct v3dvk_physical_device *dev);

void
v3dvk_physical_device_finish(struct v3dvk_physical_device *device);

#endif // V3DVK_PHYSICAL_DEVICE_H
