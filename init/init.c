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
        printf("hello from %u\n", getpid());
        fflush(stdout);
        // execl("/usr/sbin/udevd", "udevd", "--debug", NULL);
        for (;;);
    }
    if (udev_pid == -1) {
        perror("Error");
        for (;;);
    }

    printf("parent, child is %u\n", udev_pid);
    fflush(stdout);


    for (;;);
}
