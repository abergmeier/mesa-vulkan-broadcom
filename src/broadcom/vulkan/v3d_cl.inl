
#include "v3d_cl.h"
#include "v3dvk_cmd_buffer.h"

static inline uint32_t cl_offset(struct v3d_cl *cl)
{
        return (char *)cl->next - (char *)cl->base;
}

static inline void
cl_advance(struct v3d_cl_out **cl, uint32_t n)
{
        (*cl) = (struct v3d_cl_out *)((char *)(*cl) + n);
}

static inline struct v3d_cl_out *
cl_start(struct v3d_cl *cl)
{
        return cl->next;
}

static inline void
cl_end(struct v3d_cl *cl, struct v3d_cl_out *next)
{
        cl->next = next;
        assert(cl_offset(cl) <= cl->size);
}

/**
 * Reference to a BO with its associated offset, used in the pack process.
 */
static inline struct v3d_cl_reloc
cl_address(struct v3dvk_bo *bo, uint32_t offset)
{
        struct v3d_cl_reloc reloc = {
                .bo = bo,
                .offset = offset,
        };
        return reloc;
}

/**
 * Helper function called by the XML-generated pack functions for filling in
 * an address field in shader records.
 *
 * Since we have a private address space as of VC5, our BOs can have lifelong
 * offsets, and all the kernel needs to know is which BOs need to be paged in
 * for this exec.
 */
static inline void
cl_pack_emit_reloc(struct v3d_cl *cl, const struct v3d_cl_reloc *reloc)
{
   if (reloc->bo)
      v3dvk_cmd_buffer_add_bo(cl->cmd, reloc->bo);
}
