#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <libudev.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>

// Based on managarm's stage2

int main(void) {
#if 1
    int card = open("/dev/dri/card0", O_RDWR | O_CLOEXEC);
    printf("opened dri device");

    uint64_t has_dumb = 0;
    if (drmGetCap(card, DRM_CAP_DUMB_BUFFER, &has_dumb) < 0 ||
        !has_dumb) {
        fprintf(stderr, "drm device '%s' does not support dumb buffers\n",
            "/dev/dri/card0");
        close(card);
        return -1;
    }
    
    printf("\ndumb buffers supported\n");
    drmModeRes *res;
    drmModeConnector *conn;
    res = drmModeGetResources(card);
    printf("Connector with id %x\n", res->connectors[0]);
    conn = drmModeGetConnector(card, res->connectors[0]);
    printf("resolution: %dx%d\n",conn->modes[0].hdisplay, conn->modes[0].vdisplay);
    fflush(stdout);

    struct drm_mode_create_dumb creq;
    struct drm_mode_map_dumb mreq;
    creq.width = conn->modes[0].hdisplay;
	creq.height = conn->modes[0].vdisplay;
	creq.bpp = 32;
	drmIoctl(card, DRM_IOCTL_MODE_CREATE_DUMB, &creq);
    int fb_id;
    drmModeAddFB(card, conn->modes[0].hdisplay, conn->modes[0].vdisplay, 24, 32, 32, creq.handle, &fb_id);
    mreq.handle = creq.handle;
    drmIoctl(card, DRM_IOCTL_MODE_MAP_DUMB, &mreq);
    size_t addr = mmap(0, creq.size, PROT_READ | PROT_WRITE, MAP_SHARED,
		        card, mreq.offset);

    uint32_t conns[1] = {1};
    drmModeSetCrtc(card, 1, 0, 0, 0, conns, 1, &conn->modes[0]);

    printf("\ndumb buffer allocated\n");
    fflush(stdout);
#else
    printf("lyre: init started!\n");
    fflush(stdout);

    int udev_pid = fork();
    if (!udev_pid) {
        //execl("/usr/sbin/udevd", "udevd", "--debug", NULL);
        execl("/usr/bin/bash", NULL);
        for (;;);
    }
    if (udev_pid == -1) {
        perror("Error");
        for (;;);
    }

    while (access("/run/udev/rules.d", F_OK));

    printf("/run/udev/rules.d created\n");
    fflush(stdout);

    for (;;);
#endif
}
