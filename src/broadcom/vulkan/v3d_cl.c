
#include "device.h"
#include "v3d_cl.inl"
#include "common/v3d_macros.h"
#include "cle/v3d_packet_v42_pack.h"
#include "vk_alloc.h"


void
v3d_init_cl(struct v3dvk_cmd_buffer *cmd, struct v3d_cl *cl)
{
   cl->base = NULL;
   cl->next = cl->base;
   cl->size = 0;
   cl->cmd = cmd;
}

void
v3d_cl_ensure_space_with_branch(struct v3d_cl *cl, uint32_t space)
{
   if (cl_offset(cl) + space + cl_packet_length(BRANCH) <= cl->size)
      return;

   struct v3dvk_bo *new_bo = vk_alloc2(&cl->cmd->device->alloc, NULL, sizeof(*new_bo), 8,
      VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);

   if (!new_bo)
      return; // FIXME: Handle this properly

        VkResult result = v3dvk_bo_init_new(cl->cmd->device, new_bo, space, "CL");
        if (result != VK_SUCCESS) {
           // FIXME: Properly handle this
           assert(false);
        }

        assert(space <= new_bo->size);

        /* Chain to the new BO from the old one. */
        if (cl->bo) {
                cl_emit(cl, BRANCH, branch) {
                        branch.address = cl_address(new_bo, 0);
                }
#if 0
                v3d_bo_unreference(&cl->bo);
#endif
        } else {
                /* Root the first RCL/BCL BO in the job. */
                v3dvk_cmd_buffer_add_bo(cl->cmd, cl->bo);
        }

        cl->bo = new_bo;
        v3dvk_bo_map(cl->bo);
        cl->base = cl->bo->map;
        cl->size = cl->bo->size;
        cl->next = cl->base;
}

void
v3d_destroy_cl(struct v3d_cl *cl)
{
#if 0
   v3d_bo_unreference(&cl->bo);
#endif
}
