#include <dev/drm/drm.h>
#include <dev/drm/rawfb/rawfb.h>
#include <lib/builtins.h>
#include <lib/print.h>

size_t num_devices = 0;

void init_drm(void) {
    init_rawfbdev();

    return true;
}

void drm_add_device(struct drm_device* dev) {
    int n = num_devices;
    int count = 0;

    while(n) {
        n /= 10;
        count++;
    }

    char name[10 + count];
    snprint(name, 10 + count, "dri/card%d", num_devices);

    dev_add_new(dev, name);
    num_devices++;
}
