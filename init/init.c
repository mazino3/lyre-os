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

    
    printf("dumb buffers supported\n");

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
