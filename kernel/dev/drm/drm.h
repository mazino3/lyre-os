#ifndef __DRM_H__
#define __DRM_H__

#include <stdint.h>
#include <lib/resource.h>

#define DRM_IOCTL_GET_CAP       0xc010640c
#define DRM_CAP_DUMB_BUFFER		0x1

struct drm_get_cap {
	uint64_t capability;
	uint64_t value;
};

struct drm_device {
    struct resource;
};

void init_drm(void);
void drm_add_device(struct drm_device* dev);

#endif
