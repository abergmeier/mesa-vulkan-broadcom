
#ifndef V3DVK_JOB_H
#define V3DVK_JOB_H

#include "v3dvk_cl.h"

/**
 * A complete bin/render job.
 *
 * This is all of the state necessary to submit a bin/render to the kernel.
 * We want to be able to have multiple in progress at a time, so that we don't
 * need to flush an existing CL just to switch to rendering to a new render
 * target (which would mean reading back from the old render target when
 * starting to render to it again).
 */
struct v3dvk_job {
        struct v3dvk_cl bcl;
        struct v3dvk_cl rcl;
};

#endif // V3DVK_JOB_H
