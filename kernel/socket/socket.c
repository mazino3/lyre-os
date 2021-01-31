#include <stdint.h>
#include <stddef.h>
#include <socket/socket.h>
#include <socket/unix.h>
#include <socket/netlink.h>
#include <sys/cpu.h>
#include <lib/print.h>
#include <lib/errno.h>
#include <lib/resource.h>
#include <sched/sched.h>

struct resource *socket_new(int domain, int type, int protocol) {
    print("socket %u %x %u\n", domain, type, protocol);

    switch (domain) {
        case AF_UNIX:
            return unix_socket_new(type);
        case AF_NETLINK:
            return netlink_socket_new(type, protocol);
        default:
            errno = EINVAL;
            return NULL;
    }
}

void syscall_socket(struct cpu_gpr_context *ctx) {
    int domain   = (int) ctx->rdi;
    int type     = (int) ctx->rsi;
    int protocol = (int) ctx->rdx;

    struct resource *new_socket = socket_new(domain, type & SOCK_TYPE_MASK, protocol);

    if (new_socket == NULL) {
        ctx->rax = (uint64_t)-1;
        return;
    }

    int ret = fd_create_from_resource(NULL, new_socket, 0, -1);

    ctx->rax = (uint64_t)ret;
}

void syscall_bind(struct cpu_gpr_context *ctx) {
    int                    fd       = (int)                     ctx->rdi;
    const struct sockaddr *addr_ptr = (const struct sockaddr *) ctx->rsi;
    socklen_t              addr_len = (socklen_t)               ctx->rdx;

    struct resource *res = resource_from_fd(fd);
    if (res == NULL) {
        ctx->rax = (uint64_t)-1;
        return;
    }

    int ret = res->bind(res, addr_ptr, addr_len);

    ctx->rax = (uint64_t)ret;
}
