#include <lib/debug.h>
#include <sys/port_io.h>
#include <sys/cpu.h>

void syscall_debug_log(struct cpu_gpr_context *ctx) {
    debug_log((const char *)ctx->rdi);
}

void debug_log(const char *msg) {
    while (*msg) {
        outb(0xe9, *msg);
        msg++;
    }
}
