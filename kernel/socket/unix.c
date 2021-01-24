#include <stdint.h>
#include <stddef.h>
#include <socket/socket.h>
#include <socket/unix.h>
#include <lib/print.h>
#include <lib/errno.h>

struct unix_socket {
    struct resource;
};

static int unix_socket_bind(struct resource *this, const struct sockaddr *addr, socklen_t addrlen) {
    print("bind: UNIX socket\n");

    errno = EINVAL;
    return -1;
}

struct resource *unix_socket_new(int type) {
    print("socket: requested UNIX socket\n");

    struct unix_socket *new_socket = resource_create(sizeof(struct unix_socket));

    new_socket->bind = unix_socket_bind;

    return (void*)new_socket;
}
