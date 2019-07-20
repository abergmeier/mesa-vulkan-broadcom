
#include "v3d_info.h"

bool
v3d_get_param(int fd, v3d_ioctl_fun drm_ioctl, enum drm_v3d_param param, __u64* value)
{
    struct drm_v3d_get_param p = {
        .param = param,
    };
    int ret = drm_ioctl(fd, DRM_IOCTL_V3D_GET_PARAM, &p);

    if (ret != 0)
        return false;

    if (value)
        *value = p.value;

    return true;
}
