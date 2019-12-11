
#ifndef V3DVK_SHADER_H
#define V3DVK_SHADER_H

#include <stdint.h>
#include "compiler/shader_enums.h"
#include "compiler/v3d_compiler.h"
#include "v3dvk_descriptor_set.h"

struct v3dvk_device;

struct v3dvk_shader_compile_options
{
   struct v3d_key key;
#if 0
   bool optimize;
#endif
   bool include_binning_pass;
};

struct v3dvk_shader_module
{
   unsigned char sha1[20];

   uint32_t code_size;
   const uint32_t *code[0];
};

struct v3dvk_shader
{
#if 0
   struct ir3_shader ir3_shader;
#endif
   nir_shader *nir;
   gl_shader_stage type;

   struct v3dvk_descriptor_map texture_map;
   struct v3dvk_descriptor_map sampler_map;
#if 0
   struct v3dvk_descriptor_map ubo_map;

   /* This may be true for vertex shaders.  When true, variants[1] is the
    * binning variant and binning_binary is non-NULL.
    */
   bool has_binning_pass;

   void *binary;
   void *binning_binary;

   struct ir3_shader_variant variants[0];
#endif
};

struct v3dvk_shader *
v3dvk_shader_create(struct v3dvk_device *dev,
                    gl_shader_stage stage,
                    const VkPipelineShaderStageCreateInfo *stage_info,
                    const VkAllocationCallbacks *alloc);


VkResult
v3dvk_shader_compile(struct v3dvk_device *dev,
                     struct v3dvk_shader *shader,
                     const struct v3dvk_shader *next_stage,
                     const struct v3dvk_shader_compile_options *options,
                     const VkAllocationCallbacks *alloc);

void
v3dvk_shader_destroy(struct v3dvk_device *dev,
                     struct v3dvk_shader *shader,
                     const VkAllocationCallbacks *alloc);

#endif
