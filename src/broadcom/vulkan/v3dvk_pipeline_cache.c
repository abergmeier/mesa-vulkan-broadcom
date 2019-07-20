
#include <assert.h>
#include "vk_alloc.h"
#include "common.h"
#include "device.h"
#include "v3dvk_error.h"
#include "v3dvk_pipeline_cache.h"

static void
v3dvk_pipeline_cache_init(struct v3dvk_pipeline_cache *cache,
                          struct v3dvk_device *device)
{
   cache->device = device;
#if 0
   pthread_mutex_init(&cache->mutex, NULL);

   cache->modified = false;
   cache->kernel_count = 0;
   cache->total_size = 0;
   cache->table_size = 1024;
   const size_t byte_size = cache->table_size * sizeof(cache->hash_table[0]);
   cache->hash_table = malloc(byte_size);

   /* We don't consider allocation failure fatal, we just start with a 0-sized
    * cache. Disable caching when we want to keep shader debug info, since
    * we don't get the debug info on cached shaders. */
   if (cache->hash_table == NULL)
      cache->table_size = 0;
   else
      memset(cache->hash_table, 0, byte_size);
#endif
}

static void
v3dvk_pipeline_cache_finish(struct v3dvk_pipeline_cache *cache)
{
#if 0
   for (unsigned i = 0; i < cache->table_size; ++i)
      if (cache->hash_table[i]) {
         vk_free(&cache->alloc, cache->hash_table[i]);
      }
   pthread_mutex_destroy(&cache->mutex);
   free(cache->hash_table);
#endif
}

static void
v3dvk_pipeline_cache_load(struct v3dvk_pipeline_cache *cache,
                          const void *data,
                          size_t size)
{
   struct v3dvk_device *device = cache->device;
#if 0
   struct cache_header header;

   if (size < sizeof(header))
      return;
   memcpy(&header, data, sizeof(header));
   if (header.header_size < sizeof(header))
      return;
   if (header.header_version != VK_PIPELINE_CACHE_HEADER_VERSION_ONE)
      return;
   if (header.vendor_id != 0 /* TODO */)
      return;
   if (header.device_id != 0 /* TODO */)
      return;
   if (memcmp(header.uuid, device->physical_device->cache_uuid,
              VK_UUID_SIZE) != 0)
      return;

   char *end = (void *) data + size;
   char *p = (void *) data + header.header_size;

   while (end - p >= sizeof(struct cache_entry)) {
      struct cache_entry *entry = (struct cache_entry *) p;
      struct cache_entry *dest_entry;
      size_t size = entry_size(entry);
      if (end - p < size)
         break;

      dest_entry =
         vk_alloc(&cache->alloc, size, 8, VK_SYSTEM_ALLOCATION_SCOPE_CACHE);
      if (dest_entry) {
         memcpy(dest_entry, entry, size);
         for (int i = 0; i < MESA_SHADER_STAGES; ++i)
            dest_entry->variants[i] = NULL;
         tu_pipeline_cache_add_entry(cache, dest_entry);
      }
      p += size;
   }
#endif
}

VkResult
v3dvk_CreatePipelineCache(VkDevice _device,
                          const VkPipelineCacheCreateInfo *pCreateInfo,
                          const VkAllocationCallbacks *pAllocator,
                          VkPipelineCache *pPipelineCache)
{
   V3DVK_FROM_HANDLE(v3dvk_device, device, _device);
   struct v3dvk_pipeline_cache *cache;

   assert(pCreateInfo->sType == VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO);
   assert(pCreateInfo->flags == 0);

   cache = vk_alloc2(&device->alloc, pAllocator, sizeof(*cache), 8,
                     VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
   if (cache == NULL)
      return v3dvk_error(device->instance, VK_ERROR_OUT_OF_HOST_MEMORY);

   if (pAllocator)
      cache->alloc = *pAllocator;
   else
      cache->alloc = device->alloc;

   v3dvk_pipeline_cache_init(cache, device);

   if (pCreateInfo->initialDataSize > 0) {
      v3dvk_pipeline_cache_load(cache, pCreateInfo->pInitialData,
                             pCreateInfo->initialDataSize);
   }

   *pPipelineCache = v3dvk_pipeline_cache_to_handle(cache);

   return VK_SUCCESS;
}

void
v3dvk_DestroyPipelineCache(VkDevice _device,
                           VkPipelineCache _cache,
                           const VkAllocationCallbacks *pAllocator)
{
   V3DVK_FROM_HANDLE(v3dvk_device, device, _device);
   V3DVK_FROM_HANDLE(v3dvk_pipeline_cache, cache, _cache);

   if (!cache)
      return;
   v3dvk_pipeline_cache_finish(cache);

   vk_free2(&device->alloc, pAllocator, cache);
}
