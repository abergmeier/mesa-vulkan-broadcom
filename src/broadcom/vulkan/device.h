#ifndef V3DVK_DEVICE_H
#define V3DVK_DEVICE_H

#include <vulkan/vk_icd.h>
#include <vulkan/vulkan.h>

#include <xf86drm.h>

#include "dev/gen_device_info.h"

struct v3dvk_physical_device {
    VK_LOADER_DATA                              _loader_data;

    struct v3dvk_instance *                     instance;
    uint32_t                                    chipset_id;
    bool                                        no_hw;
    char                                        path[20];
    const char *                                name;
    struct {
       uint16_t                                 domain;
       uint8_t                                  bus;
       uint8_t                                  device;
       uint8_t                                  function;
    }                                           pci_info;
    struct gen_device_info                      info;
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

    struct anv_device_extension_table           supported_extensions;

    uint32_t                                    eu_total;
    uint32_t                                    subslice_total;

    struct {
      uint32_t                                  type_count;
      struct anv_memory_type                    types[VK_MAX_MEMORY_TYPES];
      uint32_t                                  heap_count;
      struct anv_memory_heap                    heaps[VK_MAX_MEMORY_HEAPS];
    } memory;
#endif
    uint8_t                                     driver_build_sha1[20];
    uint8_t                                     pipeline_cache_uuid[VK_UUID_SIZE];
    uint8_t                                     driver_uuid[VK_UUID_SIZE];
    uint8_t                                     device_uuid[VK_UUID_SIZE];
#if 0

    struct disk_cache *                         disk_cache;

    struct wsi_device                       wsi_device;
#endif
    int                                         local_fd;
    int                                         master_fd;
};

VkResult
v3dvk_physical_device_init(struct v3dvk_physical_device *device,
                         struct v3dvk_instance *instance,
                         drmDevicePtr drm_device);

void
v3dvk_physical_device_finish(struct v3dvk_physical_device *device);

#endif // V3DVK_DEVICE_H
