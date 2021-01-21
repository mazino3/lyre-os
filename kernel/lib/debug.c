#include <lib/debug.h>
#include <sys/port_io.h>

void syscall_debug_log(const char *msg) {
    debug_log(msg);
}

void debug_log(const char *msg) {
    while (*msg) {
        outb(0xe9, *msg);
        msg++;
    }
}
