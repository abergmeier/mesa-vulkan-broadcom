/*
 * Copyright © 2019 Andreas Bergmeier
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include <drm-uapi/v3d_drm.h>

#include "common.h"
#include "device.h"
#include "v3dvk_physical_device.h"
#include "ioctl.h"
#include <common/v3d_debug.h>
#include <util/build_id.h>
#include <util/macros.h>
#include <util/mesa-sha1.h>
#include <util/ralloc.h>
#include <vulkan/util/vk_util.h>
#include "v3dvk_macro.h"
#include "wsi.h"

void v3dvk_GetPhysicalDeviceFeatures(
    VkPhysicalDevice                            physicalDevice,
    VkPhysicalDeviceFeatures*                   pFeatures)
{
   V3DVK_FROM_HANDLE(v3dvk_physical_device, pdevice, physicalDevice);

   *pFeatures = (VkPhysicalDeviceFeatures) {
      .robustBufferAccess                       = false,
      .fullDrawIndexUint32                      = false,
      .imageCubeArray                           = false,
      .independentBlend                         = false,
      .geometryShader                           = false,
      .tessellationShader                       = false,
      .sampleRateShading                        = false,
      .dualSrcBlend                             = false,
      .logicOp                                  = false,
      .multiDrawIndirect                        = false,
      .drawIndirectFirstInstance                = false,
      .depthClamp                               = false,
      .depthBiasClamp                           = false,
      .fillModeNonSolid                         = false,
      .depthBounds                              = false,
      .wideLines                                = false,
      .largePoints                              = false,
      .alphaToOne                               = false,
      .multiViewport                            = false,
      .samplerAnisotropy                        = false,
      .textureCompressionETC2                   = false,
      .textureCompressionASTC_LDR               = false,
      .textureCompressionBC                     = false,
      .occlusionQueryPrecise                    = false,
      .pipelineStatisticsQuery                  = false,
      .fragmentStoresAndAtomics                 = false,
      .shaderTessellationAndGeometryPointSize   = false,
      .shaderImageGatherExtended                = false,
      .shaderStorageImageExtendedFormats        = false,
      .shaderStorageImageMultisample            = false,
      .shaderStorageImageReadWithoutFormat      = false,
      .shaderStorageImageWriteWithoutFormat     = false,
      .shaderUniformBufferArrayDynamicIndexing  = false,
      .shaderSampledImageArrayDynamicIndexing   = false,
      .shaderStorageBufferArrayDynamicIndexing  = false,
      .shaderStorageImageArrayDynamicIndexing   = false,
      .shaderClipDistance                       = false,
      .shaderCullDistance                       = false,
      .shaderFloat64                            = false,
      .shaderInt64                              = false,
      .shaderInt16                              = false,
      .shaderResourceMinLod                     = false,
      .variableMultisampleRate                  = false,
      .inheritedQueries                         = false,
   };

   pFeatures->vertexPipelineStoresAndAtomics = false;
}

void v3dvk_GetPhysicalDeviceFeatures2(
    VkPhysicalDevice                            physicalDevice,
    VkPhysicalDeviceFeatures2*                  pFeatures)
{
   V3DVK_FROM_HANDLE(v3dvk_physical_device, pdevice, physicalDevice);
   v3dvk_GetPhysicalDeviceFeatures(physicalDevice, &pFeatures->features);

   vk_foreach_struct(ext, pFeatures->pNext) {
      switch (ext->sType) {
      case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_8BIT_STORAGE_FEATURES_KHR: {
         VkPhysicalDevice8BitStorageFeaturesKHR *features =
            (VkPhysicalDevice8BitStorageFeaturesKHR *)ext;
         features->storageBuffer8BitAccess = false;
         features->uniformAndStorageBuffer8BitAccess = false;
         features->storagePushConstant8 = false;
         break;
      }

      case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_16BIT_STORAGE_FEATURES: {
         VkPhysicalDevice16BitStorageFeatures *features =
            (VkPhysicalDevice16BitStorageFeatures *)ext;
         features->storageBuffer16BitAccess = false;
         features->uniformAndStorageBuffer16BitAccess = false;
         features->storagePushConstant16 = false;
         features->storageInputOutput16 = false;
         break;
      }

      case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES_EXT: {
         VkPhysicalDeviceBufferDeviceAddressFeaturesEXT *features = (void *)ext;
         features->bufferDeviceAddress = false;
         features->bufferDeviceAddressCaptureReplay = false;
         features->bufferDeviceAddressMultiDevice = false;
         break;
      }

      case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COMPUTE_SHADER_DERIVATIVES_FEATURES_NV: {
         VkPhysicalDeviceComputeShaderDerivativesFeaturesNV *features =
            (VkPhysicalDeviceComputeShaderDerivativesFeaturesNV *)ext;
         features->computeDerivativeGroupQuads = false;
         features->computeDerivativeGroupLinear = false;
         break;
      }

      case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CONDITIONAL_RENDERING_FEATURES_EXT: {
         VkPhysicalDeviceConditionalRenderingFeaturesEXT *features =
            (VkPhysicalDeviceConditionalRenderingFeaturesEXT*)ext;
         features->conditionalRendering = false;
         features->inheritedConditionalRendering = false;
         break;
      }

      case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEPTH_CLIP_ENABLE_FEATURES_EXT: {
         VkPhysicalDeviceDepthClipEnableFeaturesEXT *features =
            (VkPhysicalDeviceDepthClipEnableFeaturesEXT *)ext;
         features->depthClipEnable = false;
         break;
      }

      case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FLOAT16_INT8_FEATURES_KHR: {
         VkPhysicalDeviceFloat16Int8FeaturesKHR *features = (void *)ext;
         features->shaderFloat16 = false;
         features->shaderInt8 = false;
         break;
      }

      case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADER_INTERLOCK_FEATURES_EXT: {
         VkPhysicalDeviceFragmentShaderInterlockFeaturesEXT *features =
            (VkPhysicalDeviceFragmentShaderInterlockFeaturesEXT *)ext;
         features->fragmentShaderSampleInterlock = false;
         features->fragmentShaderPixelInterlock = false;
         features->fragmentShaderShadingRateInterlock = false;
         break;
      }

      case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_HOST_QUERY_RESET_FEATURES_EXT: {
         VkPhysicalDeviceHostQueryResetFeaturesEXT *features =
            (VkPhysicalDeviceHostQueryResetFeaturesEXT *)ext;
         features->hostQueryReset = false;
         break;
      }

      case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT: {
         VkPhysicalDeviceDescriptorIndexingFeaturesEXT *features =
            (VkPhysicalDeviceDescriptorIndexingFeaturesEXT *)ext;
         features->shaderInputAttachmentArrayDynamicIndexing = false;
         features->shaderUniformTexelBufferArrayDynamicIndexing = false;
         features->shaderStorageTexelBufferArrayDynamicIndexing = false;
         features->shaderUniformBufferArrayNonUniformIndexing = false;
         features->shaderSampledImageArrayNonUniformIndexing = false;
         features->shaderStorageBufferArrayNonUniformIndexing = false;
         features->shaderStorageImageArrayNonUniformIndexing = false;
         features->shaderInputAttachmentArrayNonUniformIndexing = false;
         features->shaderUniformTexelBufferArrayNonUniformIndexing = false;
         features->shaderStorageTexelBufferArrayNonUniformIndexing = false;
         features->descriptorBindingUniformBufferUpdateAfterBind = false;
         features->descriptorBindingSampledImageUpdateAfterBind = false;
         features->descriptorBindingStorageImageUpdateAfterBind = false;
         features->descriptorBindingStorageBufferUpdateAfterBind = false;
         features->descriptorBindingUniformTexelBufferUpdateAfterBind = false;
         features->descriptorBindingStorageTexelBufferUpdateAfterBind = false;
         features->descriptorBindingUpdateUnusedWhilePending = false;
         features->descriptorBindingPartiallyBound = false;
         features->descriptorBindingVariableDescriptorCount = false;
         features->runtimeDescriptorArray = false;
         break;
      }

      case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_INLINE_UNIFORM_BLOCK_FEATURES_EXT: {
         VkPhysicalDeviceInlineUniformBlockFeaturesEXT *features =
            (VkPhysicalDeviceInlineUniformBlockFeaturesEXT *)ext;
         features->inlineUniformBlock = false;
         features->descriptorBindingInlineUniformBlockUpdateAfterBind = false;
         break;
      }

      case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES: {
         VkPhysicalDeviceMultiviewFeatures *features =
            (VkPhysicalDeviceMultiviewFeatures *)ext;
         features->multiview = false;
         features->multiviewGeometryShader = false;
         features->multiviewTessellationShader = false;
         break;
      }

      case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROTECTED_MEMORY_FEATURES: {
         VkPhysicalDeviceProtectedMemoryFeatures *features = (void *)ext;
         features->protectedMemory = false;
         break;
      }

      case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLER_YCBCR_CONVERSION_FEATURES: {
         VkPhysicalDeviceSamplerYcbcrConversionFeatures *features =
            (VkPhysicalDeviceSamplerYcbcrConversionFeatures *) ext;
         features->samplerYcbcrConversion = false;
         break;
      }

      case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SCALAR_BLOCK_LAYOUT_FEATURES_EXT: {
         VkPhysicalDeviceScalarBlockLayoutFeaturesEXT *features =
            (VkPhysicalDeviceScalarBlockLayoutFeaturesEXT *)ext;
         features->scalarBlockLayout = false;
         break;
      }

      case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_INT64_FEATURES_KHR: {
         VkPhysicalDeviceShaderAtomicInt64FeaturesKHR *features = (void *)ext;
         features->shaderBufferInt64Atomics = false;
         features->shaderSharedInt64Atomics = VK_FALSE;
         break;
      }

      case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DEMOTE_TO_HELPER_INVOCATION_FEATURES_EXT: {
         VkPhysicalDeviceShaderDemoteToHelperInvocationFeaturesEXT *features = (void *)ext;
         features->shaderDemoteToHelperInvocation = false;
         break;
      }

      case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DRAW_PARAMETERS_FEATURES: {
         VkPhysicalDeviceShaderDrawParametersFeatures *features = (void *)ext;
         features->shaderDrawParameters = false;
         break;
      }

      case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TEXEL_BUFFER_ALIGNMENT_FEATURES_EXT: {
         VkPhysicalDeviceTexelBufferAlignmentFeaturesEXT *features =
            (VkPhysicalDeviceTexelBufferAlignmentFeaturesEXT *)ext;
         features->texelBufferAlignment = false;
         break;
      }

      case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VARIABLE_POINTERS_FEATURES: {
         VkPhysicalDeviceVariablePointersFeatures *features = (void *)ext;
         features->variablePointersStorageBuffer = false;
         features->variablePointers = false;
         break;
      }

      case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TRANSFORM_FEEDBACK_FEATURES_EXT: {
         VkPhysicalDeviceTransformFeedbackFeaturesEXT *features =
            (VkPhysicalDeviceTransformFeedbackFeaturesEXT *)ext;
         features->transformFeedback = false;
         features->geometryStreams = false;
         break;
      }

      case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_UNIFORM_BUFFER_STANDARD_LAYOUT_FEATURES_KHR: {
         VkPhysicalDeviceUniformBufferStandardLayoutFeaturesKHR *features =
            (VkPhysicalDeviceUniformBufferStandardLayoutFeaturesKHR *)ext;
         features->uniformBufferStandardLayout = false;
         break;
      }

      case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VERTEX_ATTRIBUTE_DIVISOR_FEATURES_EXT: {
         VkPhysicalDeviceVertexAttributeDivisorFeaturesEXT *features =
            (VkPhysicalDeviceVertexAttributeDivisorFeaturesEXT *)ext;
         features->vertexAttributeInstanceRateDivisor = false;
         features->vertexAttributeInstanceRateZeroDivisor = false;
         break;
      }

      case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_YCBCR_IMAGE_ARRAYS_FEATURES_EXT: {
         VkPhysicalDeviceYcbcrImageArraysFeaturesEXT *features =
            (VkPhysicalDeviceYcbcrImageArraysFeaturesEXT *)ext;
         features->ycbcrImageArrays = false;
         break;
      }

      default:
         v3dvk_debug_ignored_stype(ext->sType);
         break;
      }
   }
}

static void
v3dvk_debug_physical_device_pci(const struct v3dvk_physical_device* dev)
{
   DBG(V3D_DEBUG_SURFACE,
       "V3DVK pci_info: %X, %X, %X, %X", dev->pci_info.domain,
                                         dev->pci_info.bus,
                                         dev->pci_info.device,
                                         dev->pci_info.function);
}

static VkResult
v3dvk_physical_device_init_uuids(struct v3dvk_physical_device *device)
{
   const struct build_id_note *note =
      build_id_find_nhdr_for_addr(v3dvk_physical_device_init_uuids);
   if (!note) {
      return vk_errorf(device->instance, device,
                       VK_ERROR_INITIALIZATION_FAILED,
                       "Failed to find build-id");
   }

   unsigned build_id_len = build_id_length(note);
   if (build_id_len < 20) {
      return vk_errorf(device->instance, device,
                       VK_ERROR_INITIALIZATION_FAILED,
                       "build-id too short.  It needs to be a SHA");
   }

   memcpy(device->driver_build_sha1, build_id_data(note), 20);

   struct mesa_sha1 sha1_ctx;
   uint8_t sha1[20];
   STATIC_ASSERT(VK_UUID_SIZE <= sizeof(sha1));

   /* The pipeline cache UUID is used for determining when a pipeline cache is
    * invalid.  It needs both a driver build and the PCI ID of the device.
    */
   _mesa_sha1_init(&sha1_ctx);
   _mesa_sha1_update(&sha1_ctx, build_id_data(note), build_id_len);
   _mesa_sha1_update(&sha1_ctx, &device->info.ver,
                     sizeof(device->info.ver));
   _mesa_sha1_update(&sha1_ctx, &device->info.vpm_size,
                     sizeof(device->info.vpm_size));
   _mesa_sha1_update(&sha1_ctx, &device->info.qpu_count,
                     sizeof(device->info.qpu_count));
#if 0
   _mesa_sha1_update(&sha1_ctx, &device->always_use_bindless,
                     sizeof(device->always_use_bindless));
   _mesa_sha1_update(&sha1_ctx, &device->has_a64_buffer_access,
                     sizeof(device->has_a64_buffer_access));
   _mesa_sha1_update(&sha1_ctx, &device->has_bindless_images,
                     sizeof(device->has_bindless_images));
   _mesa_sha1_update(&sha1_ctx, &device->has_bindless_samplers,
                     sizeof(device->has_bindless_samplers));
#endif
   _mesa_sha1_final(&sha1_ctx, sha1);
   memcpy(device->pipeline_cache_uuid, sha1, VK_UUID_SIZE);

   /* The driver UUID is used for determining sharability of images and memory
    * between two Vulkan instances in separate processes.  People who want to
    * share memory need to also check the device UUID (below) so all this
    * needs to be is the build-id.
    */
   memcpy(device->driver_uuid, build_id_data(note), VK_UUID_SIZE);

   /* The device UUID uniquely identifies the given device within the machine.
    * Since we never have more than one device, this doesn't need to be a real
    * UUID.  However, on the off-chance that someone tries to use this to
    * cache pre-tiled images or something of the like, we use the PCI ID and
    * some bits of ISL info to ensure that this is safe.
    */
   _mesa_sha1_init(&sha1_ctx);
   _mesa_sha1_update(&sha1_ctx, &device->info.ver,
                     sizeof(device->info.ver));
   _mesa_sha1_update(&sha1_ctx, &device->info.vpm_size,
                     sizeof(device->info.vpm_size));
   _mesa_sha1_update(&sha1_ctx, &device->info.qpu_count,
                     sizeof(device->info.qpu_count));
#if 0
   _mesa_sha1_update(&sha1_ctx, &device->isl_dev.has_bit6_swizzling,
                     sizeof(device->isl_dev.has_bit6_swizzling));
#endif
   _mesa_sha1_final(&sha1_ctx, sha1);
   memcpy(device->device_uuid, sha1, VK_UUID_SIZE);

   return VK_SUCCESS;
}

VkResult
v3dvk_physical_device_init(struct v3dvk_physical_device *device,
                         struct v3dvk_instance *instance,
                         drmDevicePtr drm_device)
{
   const char *primary_path = drm_device->nodes[DRM_NODE_PRIMARY];
   const char *path = drm_device->nodes[DRM_NODE_RENDER];
   VkResult result;
   int fd;
   int master_fd = -1;

   fd = open(path, O_RDWR | O_CLOEXEC);
   if (fd < 0)
      return vk_error(VK_ERROR_INCOMPATIBLE_DRIVER);

   device->_loader_data.loaderMagic = ICD_LOADER_MAGIC;
   device->instance = instance;

   assert(strlen(path) < ARRAY_SIZE(device->path));
   snprintf(device->path, ARRAY_SIZE(device->path), "%s", path);

   if (!v3d_get_device_info(fd, &device->info, drmIoctl)) {
      result = vk_error(VK_ERROR_INCOMPATIBLE_DRIVER);
      goto fail;
   }

   device->pci_info.domain = drm_device->businfo.pci->domain;
   device->pci_info.bus = drm_device->businfo.pci->bus;
   device->pci_info.device = drm_device->businfo.pci->dev;
   device->pci_info.function = drm_device->businfo.pci->func;

   v3dvk_debug_physical_device_pci(device);

   device->name = v3d_get_device_name(&device->info);
   if (!device->name) {
      result = vk_error(VK_ERROR_INCOMPATIBLE_DRIVER);
      goto fail;
   }

   if (device->info.ver == 42) {
      /* Videocore 6 fully supported */
   } else {
      result = vk_errorf(device->instance, device,
                         VK_ERROR_INCOMPATIBLE_DRIVER,
                         "Vulkan not yet supported on %s", device->name);
      goto fail;
   }

#if 0
   result = v3dvk_physical_device_init_heaps(device, fd);
   if (result != VK_SUCCESS)
      goto fail;
#endif

   result = v3dvk_physical_device_init_uuids(device);
   if (result != VK_SUCCESS)
      goto fail;
#if 0
   if (instance->enabled_extensions.KHR_display) {
#else
   if (0) {
#endif
      master_fd = open(primary_path, O_RDWR | O_CLOEXEC);
#if 0
      if (master_fd >= 0) {
         /* prod the device with a GETPARAM call which will fail if
          * we don't have permission to even render on this device
          */
         if (anv_gem_get_param(master_fd, I915_PARAM_CHIPSET_ID) == 0) {
            close(master_fd);
            master_fd = -1;
         }
      }
#endif
   }

   device->master_fd = master_fd;

   result = v3dvk_init_wsi(device);
   if (result != VK_SUCCESS) {
      goto fail;
   }

   device->local_fd = fd;

   return VK_SUCCESS;

fail:
   close(fd);
   if (master_fd != -1)
      close(master_fd);
   return result;
}

void
v3dvk_physical_device_finish(struct v3dvk_physical_device *device)
{
   v3dvk_finish_wsi(device);
   close(device->local_fd);
   if (device->master_fd >= 0)
      close(device->master_fd);
}

void v3dvk_GetPhysicalDeviceProperties(
    VkPhysicalDevice                            physicalDevice,
    VkPhysicalDeviceProperties*                 pProperties)
{
   V3DVK_FROM_HANDLE(v3dvk_physical_device, pdevice, physicalDevice);
   const struct v3d_device_info *devinfo = &pdevice->info;

   VkPhysicalDeviceLimits limits;
   // TODO: Insert actual values
   for (uint8_t* bl = (uint8_t*)&limits; bl < (uint8_t*)&limits + sizeof(VkPhysicalDeviceLimits); ++bl) {
      *bl = 0;
   }
#if 0
   VkPhysicalDeviceLimits limits = {
      .maxImageDimension1D                      = (1 << 14),
      .maxImageDimension2D                      = (1 << 14),
      .maxImageDimension3D                      = (1 << 11),
      .maxImageDimensionCube                    = (1 << 14),
      .maxImageArrayLayers                      = (1 << 11),
      .maxTexelBufferElements                   = 128 * 1024 * 1024,
      .maxUniformBufferRange                    = (1ul << 27),
      .maxStorageBufferRange                    = max_raw_buffer_sz,
      .maxPushConstantsSize                     = MAX_PUSH_CONSTANTS_SIZE,
      .maxMemoryAllocationCount                 = UINT32_MAX,
      .maxSamplerAllocationCount                = 64 * 1024,
      .bufferImageGranularity                   = 64, /* A cache line */
      .sparseAddressSpaceSize                   = 0,
      .maxBoundDescriptorSets                   = MAX_SETS,
      .maxPerStageDescriptorSamplers            = max_samplers,
      .maxPerStageDescriptorUniformBuffers      = MAX_PER_STAGE_DESCRIPTOR_UNIFORM_BUFFERS,
      .maxPerStageDescriptorStorageBuffers      = max_ssbos,
      .maxPerStageDescriptorSampledImages       = max_textures,
      .maxPerStageDescriptorStorageImages       = max_images,
      .maxPerStageDescriptorInputAttachments    = MAX_PER_STAGE_DESCRIPTOR_INPUT_ATTACHMENTS,
      .maxPerStageResources                     = max_per_stage,
      .maxDescriptorSetSamplers                 = 6 * max_samplers, /* number of stages * maxPerStageDescriptorSamplers */
      .maxDescriptorSetUniformBuffers           = 6 * MAX_PER_STAGE_DESCRIPTOR_UNIFORM_BUFFERS,           /* number of stages * maxPerStageDescriptorUniformBuffers */
      .maxDescriptorSetUniformBuffersDynamic    = MAX_DYNAMIC_BUFFERS / 2,
      .maxDescriptorSetStorageBuffers           = 6 * max_ssbos,    /* number of stages * maxPerStageDescriptorStorageBuffers */
      .maxDescriptorSetStorageBuffersDynamic    = MAX_DYNAMIC_BUFFERS / 2,
      .maxDescriptorSetSampledImages            = 6 * max_textures, /* number of stages * maxPerStageDescriptorSampledImages */
      .maxDescriptorSetStorageImages            = 6 * max_images,   /* number of stages * maxPerStageDescriptorStorageImages */
      .maxDescriptorSetInputAttachments         = MAX_DESCRIPTOR_SET_INPUT_ATTACHMENTS,
      .maxVertexInputAttributes                 = MAX_VBS,
      .maxVertexInputBindings                   = MAX_VBS,
      .maxVertexInputAttributeOffset            = 2047,
      .maxVertexInputBindingStride              = 2048,
      .maxVertexOutputComponents                = 128,
      .maxTessellationGenerationLevel           = 64,
      .maxTessellationPatchSize                 = 32,
      .maxTessellationControlPerVertexInputComponents = 128,
      .maxTessellationControlPerVertexOutputComponents = 128,
      .maxTessellationControlPerPatchOutputComponents = 128,
      .maxTessellationControlTotalOutputComponents = 2048,
      .maxTessellationEvaluationInputComponents = 128,
      .maxTessellationEvaluationOutputComponents = 128,
      .maxGeometryShaderInvocations             = 32,
      .maxGeometryInputComponents               = 64,
      .maxGeometryOutputComponents              = 128,
      .maxGeometryOutputVertices                = 256,
      .maxGeometryTotalOutputComponents         = 1024,
      .maxFragmentInputComponents               = 116, /* 128 components - (PSIZ, CLIP_DIST0, CLIP_DIST1) */
      .maxFragmentOutputAttachments             = 8,
      .maxFragmentDualSrcAttachments            = 1,
      .maxFragmentCombinedOutputResources       = 8,
      .maxComputeSharedMemorySize               = 64 * 1024,
      .maxComputeWorkGroupCount                 = { 65535, 65535, 65535 },
      .maxComputeWorkGroupInvocations           = 32 * devinfo->max_cs_threads,
      .maxComputeWorkGroupSize = {
         16 * devinfo->max_cs_threads,
         16 * devinfo->max_cs_threads,
         16 * devinfo->max_cs_threads,
      },
      .subPixelPrecisionBits                    = 8,
      .subTexelPrecisionBits                    = 8,
      .mipmapPrecisionBits                      = 8,
      .maxDrawIndexedIndexValue                 = UINT32_MAX,
      .maxDrawIndirectCount                     = UINT32_MAX,
      .maxSamplerLodBias                        = 16,
      .maxSamplerAnisotropy                     = 16,
      .maxViewports                             = MAX_VIEWPORTS,
      .maxViewportDimensions                    = { (1 << 14), (1 << 14) },
      .viewportBoundsRange                      = { INT16_MIN, INT16_MAX },
      .viewportSubPixelBits                     = 13, /* We take a float? */
      .minMemoryMapAlignment                    = 4096, /* A page */
      /* The dataport requires texel alignment so we need to assume a worst
       * case of R32G32B32A32 which is 16 bytes.
       */
      .minTexelBufferOffsetAlignment            = 16,
      /* We need 16 for UBO block reads to work and 32 for push UBOs */
      .minUniformBufferOffsetAlignment          = 32,
      .minStorageBufferOffsetAlignment          = 4,
      .minTexelOffset                           = -8,
      .maxTexelOffset                           = 7,
      .minTexelGatherOffset                     = -32,
      .maxTexelGatherOffset                     = 31,
      .minInterpolationOffset                   = -0.5,
      .maxInterpolationOffset                   = 0.4375,
      .subPixelInterpolationOffsetBits          = 4,
      .maxFramebufferWidth                      = (1 << 14),
      .maxFramebufferHeight                     = (1 << 14),
      .maxFramebufferLayers                     = (1 << 11),
      .framebufferColorSampleCounts             = sample_counts,
      .framebufferDepthSampleCounts             = sample_counts,
      .framebufferStencilSampleCounts           = sample_counts,
      .framebufferNoAttachmentsSampleCounts     = sample_counts,
      .maxColorAttachments                      = MAX_RTS,
      .sampledImageColorSampleCounts            = sample_counts,
      .sampledImageIntegerSampleCounts          = VK_SAMPLE_COUNT_1_BIT,
      .sampledImageDepthSampleCounts            = sample_counts,
      .sampledImageStencilSampleCounts          = sample_counts,
      .storageImageSampleCounts                 = VK_SAMPLE_COUNT_1_BIT,
      .maxSampleMaskWords                       = 1,
      .timestampComputeAndGraphics              = true,
      .timestampPeriod                          = 1000000000.0 / devinfo->timestamp_frequency,
      .maxClipDistances                         = 8,
      .maxCullDistances                         = 8,
      .maxCombinedClipAndCullDistances          = 8,
      .discreteQueuePriorities                  = 2,
      .pointSizeRange                           = { 0.125, 255.875 },
      .lineWidthRange                           = { 0.0, 7.9921875 },
      .pointSizeGranularity                     = (1.0 / 8.0),
      .lineWidthGranularity                     = (1.0 / 128.0),
      .strictLines                              = false, /* FINISHME */
      .standardSampleLocations                  = true,
      .optimalBufferCopyOffsetAlignment         = 128,
      .optimalBufferCopyRowPitchAlignment       = 128,
      .nonCoherentAtomSize                      = 64,
   };
#endif
   *pProperties = (VkPhysicalDeviceProperties) {
      .apiVersion = v3dvk_physical_device_api_version(pdevice),
      .driverVersion = vk_get_driver_version(),
      .vendorID = 0x8086,
      .deviceID = pdevice->info.ver,
      .deviceType = VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU,
      .limits = limits,
      .sparseProperties = {0}, /* TODO: See whether we can do sparse */
   };

   snprintf(pProperties->deviceName, sizeof(pProperties->deviceName),
            "%s", pdevice->name);
   memcpy(pProperties->pipelineCacheUUID,
          pdevice->pipeline_cache_uuid, VK_UUID_SIZE);
}

/* For now we support exactly one queue family. */
static const VkQueueFamilyProperties
v3dvk_queue_family_properties = {
   .queueFlags = VK_QUEUE_GRAPHICS_BIT |
                 VK_QUEUE_COMPUTE_BIT |
                 VK_QUEUE_TRANSFER_BIT,
   .queueCount = 1,
   .timestampValidBits = 36, /* XXX: Real value here */
   .minImageTransferGranularity = { 1, 1, 1 },
};

void v3dvk_GetPhysicalDeviceQueueFamilyProperties(
    VkPhysicalDevice                              physicalDevice,
    uint32_t*                                     pCount,
    VkQueueFamilyProperties*                      pQueueFamilyProperties)
{
   VK_OUTARRAY_MAKE(out, pQueueFamilyProperties, pCount);

   vk_outarray_append(&out, p) {
      *p = v3dvk_queue_family_properties;
   }
}

void v3dvk_GetPhysicalDeviceMemoryProperties(
    VkPhysicalDevice                            physicalDevice,
    VkPhysicalDeviceMemoryProperties*           pMemoryProperties)
{
   V3DVK_FROM_HANDLE(v3dvk_physical_device, physical_device, physicalDevice);

   pMemoryProperties->memoryTypeCount = physical_device->memory.type_count;
   for (uint32_t i = 0; i < physical_device->memory.type_count; i++) {
      pMemoryProperties->memoryTypes[i] = (VkMemoryType) {
         .propertyFlags = physical_device->memory.types[i].propertyFlags,
         .heapIndex     = physical_device->memory.types[i].heapIndex,
      };
   }

   pMemoryProperties->memoryHeapCount = physical_device->memory.heap_count;
   for (uint32_t i = 0; i < physical_device->memory.heap_count; i++) {
      pMemoryProperties->memoryHeaps[i] = (VkMemoryHeap) {
         .size    = physical_device->memory.heaps[i].size,
         .flags   = physical_device->memory.heaps[i].flags,
      };
   }
}

PFN_vkVoidFunction v3dvk_GetDeviceProcAddr(
    VkDevice                                    _device,
    const char*                                 pName)
{
   V3DVK_FROM_HANDLE(v3dvk_device, device, _device);

   if (!device || !pName)
      return NULL;

   int idx = v3dvk_get_device_entrypoint_index(pName);
   if (idx < 0)
      return NULL;

   return device->dispatch.entrypoints[idx];
}

VkResult v3dvk_EnumerateDeviceExtensionProperties(
    VkPhysicalDevice                              physicalDevice,
    const char*                                   pLayerName,
    uint32_t*                                     pPropertyCount,
    VkExtensionProperties*                        pProperties)
{
   V3DVK_FROM_HANDLE(v3dvk_physical_device, device, physicalDevice);
   VK_OUTARRAY_MAKE(out, pProperties, pPropertyCount);

   for (int i = 0; i < V3DVK_DEVICE_EXTENSION_COUNT; i++) {
      if (device->supported_extensions.extensions[i]) {
         vk_outarray_append(&out, prop) {
            *prop = v3dvk_device_extensions[i];
         }
      }
   }

   return vk_outarray_status(&out);
}
