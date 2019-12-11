
#include "compiler/spirv/nir_spirv.h"
#include "vk_alloc.h"
#include "common.h"
#include "device.h"
#include "instance.h"
#include "v3dvk_error.h"
#include "v3dvk_shader.h"
#include "util/mesa-sha1.h"

static nir_shader *
v3dvk_spirv_to_nir(const uint32_t *words,
                   size_t word_count,
                   gl_shader_stage stage,
                   const char *entry_point_name,
                   const VkSpecializationInfo *spec_info)
{
   /* TODO these are made-up */
   const struct spirv_to_nir_options spirv_options = {
      .frag_coord_is_sysval = true,
      .lower_ubo_ssbo_access_to_offsets = true,
      .caps = { false },
   };

   /* convert VkSpecializationInfo */
   struct nir_spirv_specialization *spec = NULL;
   uint32_t num_spec = 0;
   if (spec_info && spec_info->mapEntryCount) {
      spec = malloc(sizeof(*spec) * spec_info->mapEntryCount);
      if (!spec)
         return NULL;

      for (uint32_t i = 0; i < spec_info->mapEntryCount; i++) {
         const VkSpecializationMapEntry *entry = &spec_info->pMapEntries[i];
         const void *data = spec_info->pData + entry->offset;
         assert(data + entry->size <= spec_info->pData + spec_info->dataSize);
         spec[i].id = entry->constantID;
         if (entry->size == 8)
            spec[i].data64 = *(const uint64_t *) data;
         else
            spec[i].data32 = *(const uint32_t *) data;
         spec[i].defined_on_module = false;
      }

      num_spec = spec_info->mapEntryCount;
   }

   nir_shader *nir =
      spirv_to_nir(words, word_count, spec, num_spec, stage, entry_point_name,
                   &spirv_options, &v3d_nir_options);

   free(spec);

   assert(nir->info.stage == stage);
   nir_validate_shader(nir, "after spirv_to_nir");

   return nir;
}

static void
v3dvk_sort_variables_by_location(struct exec_list *variables)
{
   struct exec_list sorted;
   exec_list_make_empty(&sorted);

   nir_foreach_variable_safe(var, variables)
   {
      exec_node_remove(&var->node);

      /* insert the variable into the sorted list */
      nir_variable *next = NULL;
      nir_foreach_variable(tmp, &sorted)
      {
         if (var->data.location < tmp->data.location) {
            next = tmp;
            break;
         }
      }
      if (next)
         exec_node_insert_node_before(&next->node, &var->node);
      else
         exec_list_push_tail(&sorted, &var->node);
   }

   exec_list_move_nodes_to(&sorted, variables);
}

struct v3dvk_shader *
v3dvk_shader_create(struct v3dvk_device *dev,
                    gl_shader_stage stage,
                    const VkPipelineShaderStageCreateInfo *stage_info,
                    const VkAllocationCallbacks *alloc) {

   const struct v3dvk_shader_module *module =
      v3dvk_shader_module_from_handle(stage_info->module);
   struct v3dvk_shader *shader;

   const uint32_t max_variant_count = (stage == MESA_SHADER_VERTEX) ? 2 : 1;
   shader = vk_zalloc2(
      &dev->alloc, alloc,
      sizeof(*shader),
      8, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND);
   if (!shader)
      return NULL;

   /* translate SPIR-V to NIR */
   assert(module->code_size % 4 == 0);
   nir_shader *nir = v3dvk_spirv_to_nir(
      (const uint32_t *) module->code, module->code_size / 4,
      stage, stage_info->pName, stage_info->pSpecializationInfo);
   if (!nir) {
      vk_free2(&dev->alloc, alloc, shader);
      return NULL;
   }

   if (unlikely(dev->instance->debug_flags & V3DVK_DEBUG_NIR)) {
      fprintf(stderr, "translated nir:\n");
      nir_print_shader(nir, stderr);
   }

      /* multi step inlining procedure */
   NIR_PASS_V(nir, nir_lower_constant_initializers, nir_var_function_temp);
   NIR_PASS_V(nir, nir_lower_returns);
   NIR_PASS_V(nir, nir_inline_functions);
   NIR_PASS_V(nir, nir_opt_deref);
   foreach_list_typed_safe(nir_function, func, node, &nir->functions) {
      if (!func->is_entrypoint)
         exec_node_remove(&func->node);
   }
   assert(exec_list_length(&nir->functions) == 1);
   NIR_PASS_V(nir, nir_lower_constant_initializers, ~nir_var_function_temp);

   /* Split member structs.  We do this before lower_io_to_temporaries so that
    * it doesn't lower system values to temporaries by accident.
    */
   NIR_PASS_V(nir, nir_split_var_copies);
   NIR_PASS_V(nir, nir_split_per_member_structs);

   NIR_PASS_V(nir, nir_remove_dead_variables,
              nir_var_shader_in | nir_var_shader_out | nir_var_system_value | nir_var_mem_shared);

   NIR_PASS_V(nir, nir_propagate_invariant);

   NIR_PASS_V(nir, nir_lower_io_to_temporaries, nir_shader_get_entrypoint(nir), true, true);

   NIR_PASS_V(nir, nir_lower_global_vars_to_local);
   NIR_PASS_V(nir, nir_split_var_copies);
   NIR_PASS_V(nir, nir_lower_var_copies);

   NIR_PASS_V(nir, nir_opt_copy_prop_vars);
   NIR_PASS_V(nir, nir_opt_combine_stores, nir_var_all);
#if 0
   /* ir3 doesn't support indirect input/output */
   NIR_PASS_V(nir, nir_lower_indirect_derefs, nir_var_shader_in | nir_var_shader_out);
#endif
   switch (stage) {
   case MESA_SHADER_VERTEX:
      v3dvk_sort_variables_by_location(&nir->outputs);
      break;
   case MESA_SHADER_TESS_CTRL:
   case MESA_SHADER_TESS_EVAL:
   case MESA_SHADER_GEOMETRY:
      v3dvk_sort_variables_by_location(&nir->inputs);
      v3dvk_sort_variables_by_location(&nir->outputs);
      break;
   case MESA_SHADER_FRAGMENT:
      v3dvk_sort_variables_by_location(&nir->inputs);
      break;
   case MESA_SHADER_COMPUTE:
      break;
   default:
      unreachable("invalid gl_shader_stage");
      break;
   }

   nir_assign_io_var_locations(&nir->inputs, &nir->num_inputs, stage);
   nir_assign_io_var_locations(&nir->outputs, &nir->num_outputs, stage);

   NIR_PASS_V(nir, nir_lower_system_values);
   NIR_PASS_V(nir, nir_lower_frexp);
#if 0
   NIR_PASS_V(nir, tu_lower_io, shader);

   NIR_PASS_V(nir, nir_lower_io, nir_var_all, ir3_glsl_type_size, 0);
#endif
#if 0
   if (stage == MESA_SHADER_FRAGMENT) {
      /* NOTE: lower load_barycentric_at_sample first, since it
       * produces load_barycentric_at_offset:
       */
      NIR_PASS_V(nir, ir3_nir_lower_load_barycentric_at_sample);
      NIR_PASS_V(nir, ir3_nir_lower_load_barycentric_at_offset);

      NIR_PASS_V(nir, ir3_nir_move_varying_inputs);
   }
#endif
   NIR_PASS_V(nir, nir_lower_io_arrays_to_elements_no_indirects, false);

   nir_shader_gather_info(nir, nir_shader_get_entrypoint(nir));

   /* num_uniforms only used by ir3 for size of ubo 0 (push constants) */
   nir->num_uniforms = MAX_PUSH_CONSTANTS_SIZE / 16;
#if 0
   shader->ir3_shader.compiler = dev->compiler;
#endif
   shader->type = stage;
   shader->nir = nir;

   return shader;
}

static void
v3dvk_shader_debug_output(const char *message, void *data)
{
   struct v3dvk_device *dev = data;

   // TODO: Make output conditional
   fprintf(stderr, "SHADER_INFO %s:\n", message);
}

#if 0
static uint32_t *
v3dvk_compile_shader_variant(struct v3dvk_device* dev,
                             struct v3d_key *key,
                          struct ir3_shader_variant *nonbinning,
                          struct ir3_shader_variant *variant)
{
   variant->shader = shader;
   variant->type = shader->type;
   variant->key = *key;
   variant->binning_pass = !!nonbinning;
   variant->nonbinning = nonbinning;

   nir_shader *s

   uint64_t *qpu_insts;
   uint32_t shader_size;

   qpu_insts = v3d_compile(dev->compiler,
                      key,
                      struct v3d_prog_data **prog_data,
                      nir_shader *s,
                      v3dvk_shader_debug_output,
                      dev,
                      int program_id, int variant_id,
                      &shader_size);

   free(qpu_insts);

   // TODO: Implement cache

}
#endif

VkResult
v3dvk_shader_compile(struct v3dvk_device *dev,
                     struct v3dvk_shader *shader,
                     const struct v3dvk_shader *next_stage,
                     const struct v3dvk_shader_compile_options *options,
                     const VkAllocationCallbacks *alloc)
{
#if 0
   if (options->optimize) {
      /* ignore the key for the first pass of optimization */
      ir3_optimize_nir(&shader->ir3_shader, shader->ir3_shader.nir, NULL);

      if (unlikely(dev->physical_device->instance->debug_flags &
                   TU_DEBUG_NIR)) {
         fprintf(stderr, "optimized nir:\n");
         nir_print_shader(shader->ir3_shader.nir, stderr);
      }
   }
#endif
#if 0
   shader->binary = v3dvk_compile_shader_variant(
      dev,
      &shader->ir3_shader, &options->key, NULL, &shader->variants[0]);
   if (!shader->binary)
      return VK_ERROR_OUT_OF_HOST_MEMORY;

   /* compile another variant for the binning pass */
   if (options->include_binning_pass &&
       shader->ir3_shader.type == MESA_SHADER_VERTEX) {
      shader->binning_binary = tu_compile_shader_variant(
         &shader->ir3_shader, &options->key, &shader->variants[0],
         &shader->variants[1]);
      if (!shader->binning_binary)
         return VK_ERROR_OUT_OF_HOST_MEMORY;

      shader->has_binning_pass = true;
   }

   if (unlikely(dev->instance->debug_flags & V3DVK_DEBUG_IR3)) {
      fprintf(stderr, "disassembled ir3:\n");
      fprintf(stderr, "shader: %s\n",
              gl_shader_stage_name(shader->ir3_shader.type));
      ir3_shader_disasm(&shader->variants[0], shader->binary, stderr);

      if (shader->has_binning_pass) {
         fprintf(stderr, "disassembled ir3:\n");
         fprintf(stderr, "shader: %s (binning)\n",
                 gl_shader_stage_name(shader->ir3_shader.type));
         ir3_shader_disasm(&shader->variants[1], shader->binning_binary,
                           stderr);
      }
   }

#endif
   return VK_SUCCESS;
}

VkResult
v3dvk_CreateShaderModule(VkDevice _device,
                         const VkShaderModuleCreateInfo *pCreateInfo,
                         const VkAllocationCallbacks *pAllocator,
                         VkShaderModule *pShaderModule)
{
   V3DVK_FROM_HANDLE(v3dvk_device, device, _device);
   struct v3dvk_shader_module *module;

   assert(pCreateInfo->sType == VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO);
   assert(pCreateInfo->flags == 0);
   assert(pCreateInfo->codeSize % 4 == 0);

   module = vk_alloc2(&device->alloc, pAllocator,
                      sizeof(*module) + pCreateInfo->codeSize, 8,
                      VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
   if (module == NULL)
      return v3dvk_error(device->instance, VK_ERROR_OUT_OF_HOST_MEMORY);

   module->code_size = pCreateInfo->codeSize;
   memcpy(module->code, pCreateInfo->pCode, pCreateInfo->codeSize);

   _mesa_sha1_compute(module->code, module->code_size, module->sha1);

   *pShaderModule = v3dvk_shader_module_to_handle(module);

   return VK_SUCCESS;
}

void
v3dvk_DestroyShaderModule(VkDevice _device,
                          VkShaderModule _module,
                          const VkAllocationCallbacks *pAllocator)
{
   V3DVK_FROM_HANDLE(v3dvk_device, device, _device);
   V3DVK_FROM_HANDLE(v3dvk_shader_module, module, _module);

   if (!module)
      return;

   vk_free2(&device->alloc, pAllocator, module);
}
