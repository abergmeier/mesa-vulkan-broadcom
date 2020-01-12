#ifndef V3DVK_PIPELINE_H
#define V3DVK_PIPELINE_H

struct v3dvk_pipeline
{
#if 0
   struct tu_cs cs;

   struct tu_dynamic_state dynamic_state;

   struct tu_pipeline_layout *layout;

   bool need_indirect_descriptor_sets;
   VkShaderStageFlags active_stages;

   struct
   {
      struct tu_bo binary_bo;
      struct tu_cs_entry state_ib;
      struct tu_cs_entry binning_state_ib;

      struct tu_program_descriptor_linkage link[MESA_SHADER_STAGES];
   } program;

   struct
   {
      uint8_t bindings[MAX_VERTEX_ATTRIBS];
      uint16_t strides[MAX_VERTEX_ATTRIBS];
      uint16_t offsets[MAX_VERTEX_ATTRIBS];
      uint32_t count;

      uint8_t binning_bindings[MAX_VERTEX_ATTRIBS];
      uint16_t binning_strides[MAX_VERTEX_ATTRIBS];
      uint16_t binning_offsets[MAX_VERTEX_ATTRIBS];
      uint32_t binning_count;

      struct tu_cs_entry state_ib;
      struct tu_cs_entry binning_state_ib;
   } vi;

   struct
   {
      enum pc_di_primtype primtype;
      bool primitive_restart;
   } ia;

   struct
   {
      struct tu_cs_entry state_ib;
   } vp;

   struct
   {
      uint32_t gras_su_cntl;
      struct tu_cs_entry state_ib;
   } rast;

   struct
   {
      struct tu_cs_entry state_ib;
   } ds;

   struct
   {
      struct tu_cs_entry state_ib;
   } blend;
#endif
};

#endif
