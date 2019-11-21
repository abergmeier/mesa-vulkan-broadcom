
#ifndef V3DVK_SAMPLER_H
#define V3DVK_SAMPLER_H

// pack header implicitly depends on cl header
#include "v3d_cl.h"
#include <cle/v3d_packet_v42_pack.h>

struct v3dvk_sampler {
	struct V3D42_SAMPLER_STATE state;
};

#endif
