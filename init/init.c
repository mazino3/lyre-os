#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <libudev.h>

// Based on managarm's stage2

int main(void) {
    printf("lyre: init started!\n");
    fflush(stdout);

    int udev_pid = fork();
    if (!udev_pid) {
        execl("/usr/sbin/udevd", "udevd", "--debug", NULL);
        for (;;);
    }
    if (udev_pid == -1) {
        perror("Error");
        for (;;);
    }


    for (;;);
}
