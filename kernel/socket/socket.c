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

    struct handle *handle = alloc(sizeof(struct handle));

    handle->res = new_socket;
    handle->loc = 0;

    struct process *process = this_cpu->current_thread->process;

    int ret = DYNARRAY_INSERT(process->handles, handle);

    ctx->rax = (uint64_t)ret;
}

void syscall_bind(struct cpu_gpr_context *ctx) {
    int                    fd       = (int)                     ctx->rdi;
    const struct sockaddr *addr_ptr = (const struct sockaddr *) ctx->rsi;
    socklen_t              addr_len = (socklen_t)               ctx->rdx;

    struct process *process = this_cpu->current_thread->process;

    struct handle *handle = process->handles.storage[fd];

    int ret = handle->res->bind(handle->res, addr_ptr, addr_len);

    ctx->rax = (uint64_t)ret;
}
