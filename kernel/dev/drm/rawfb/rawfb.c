#include <dev/drm/rawfb/rawfb.h>
#include <dev/drm/drm.h>
#include <dev/dev.h>
#include <lib/resource.h>

#include <lib/print.h>

static int rawfb_ioctl(struct resource *this, int request, void *argp) {
    switch (request) {
        case DRM_IOCTL_GET_CAP : {
            print("drm get capability\n");
            struct drm_get_cap *cap = argp;
            if (cap->capability == DRM_CAP_DUMB_BUFFER) {
                cap->value = 1;
                return 0;
            }
        }
    }
}

void init_rawfbdev(void) {
    struct drm_device *dri = resource_create(sizeof(struct drm_device));
    dri->ioctl = rawfb_ioctl;

    drm_add_device(dri);
}
