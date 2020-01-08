/*
 * Copyright © 2014-2017 Broadcom
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

#ifndef VC5_CL_H
#define VC5_CL_H

#include <stdint.h>
#include "v3dvk_bo.h"

#include "broadcom/cle/v3d_packet_helpers.h"


struct v3d_job;
struct v3d_cl;
struct v3dvk_device;

/**
 * Undefined structure, used for typechecking that you're passing the pointers
 * to these functions correctly.
 */
struct v3d_cl_out;

/** A reference to a BO used in the CL packing functions */
struct v3d_cl_reloc {
        struct v3dvk_bo *bo;
        uint32_t offset;
};

static inline struct v3d_cl_reloc
__unpack_address(const uint8_t *cl, uint32_t s, uint32_t e)
{
    struct v3d_cl_reloc reloc =
            { NULL, __gen_unpack_uint(cl, s, e) << (31 - (e - s)) };
    return reloc;
}

static inline void cl_pack_emit_reloc(struct v3d_cl *cl, const struct v3d_cl_reloc *);

#define __gen_user_data struct v3d_cl
#define __gen_address_type struct v3d_cl_reloc
#define __gen_address_offset(reloc) (((reloc)->bo ? (reloc)->bo->offset : 0) + \
                                     (reloc)->offset)
#define __gen_emit_reloc cl_pack_emit_reloc
#define __gen_unpack_address(cl, s, e) __unpack_address(cl, s, e)

struct v3d_cl {
        void *base;
        struct v3dvk_cmd_buffer *cmd;
        struct v3d_cl_out *next;
        struct v3dvk_bo *bo;
        uint32_t size;
};

void v3d_init_cl(struct v3dvk_cmd_buffer *cmd, struct v3d_cl *cl);
void v3d_destroy_cl(struct v3d_cl *cl);

void v3d_cl_ensure_space_with_branch(struct v3d_cl *cl, uint32_t size);

#define cl_packet_header(packet) V3D42_ ## packet ## _header
#define cl_packet_length(packet) V3D42_ ## packet ## _length
#define cl_packet_pack(packet)   V3D42_ ## packet ## _pack
#define cl_packet_struct(packet) V3D42_ ## packet

static void
cl_advance(struct v3d_cl_out **cl, uint32_t n);
static struct v3d_cl_out *
cl_start(struct v3d_cl *cl);
static void
cl_end(struct v3d_cl *cl, struct v3d_cl_out *next);


/* Macro for setting up an emit of a CL struct.  A temporary unpacked struct
 * is created, which you get to set fields in of the form:
 *
 * cl_emit(bcl, FLAT_SHADE_FLAGS, flags) {
 *     .flags.flat_shade_flags = 1 << 2,
 * }
 *
 * or default values only can be emitted with just:
 *
 * cl_emit(bcl, FLAT_SHADE_FLAGS, flags);
 *
 * The trick here is that we make a for loop that will execute the body
 * (either the block or the ';' after the macro invocation) exactly once.
 */
#define cl_emit(cl, packet, name)                                \
        for (struct cl_packet_struct(packet) name = {            \
                cl_packet_header(packet)                         \
        },                                                       \
        *_loop_terminate = &name;                                \
        __builtin_expect(_loop_terminate != NULL, 1);            \
        ({                                                       \
                struct v3d_cl_out *cl_out = cl_start(cl);        \
                cl_packet_pack(packet)(cl, (uint8_t *)cl_out, &name); \
                cl_advance(&cl_out, cl_packet_length(packet));   \
                cl_end(cl, cl_out);                              \
                _loop_terminate = NULL;                          \
        }))


#define v3dx_pack(packed, packet, name)                          \
        for (struct cl_packet_struct(packet) name = {            \
                cl_packet_header(packet)                         \
        },                                                       \
        *_loop_terminate = &name;                                \
        __builtin_expect(_loop_terminate != NULL, 1);            \
        ({                                                       \
                cl_packet_pack(packet)(NULL, (uint8_t *)packed, &name); \
                VG(VALGRIND_CHECK_MEM_IS_DEFINED((uint8_t *)packed, \
                                                 cl_packet_length(packet))); \
                _loop_terminate = NULL;                          \
        }))                                                      \

#endif /* VC5_CL_H */
