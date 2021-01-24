#include <stdint.h>
#include <stddef.h>
#include <socket/socket.h>
#include <socket/netlink.h>
#include <lib/print.h>
#include <lib/errno.h>

struct netlink_socket {
    struct resource;

    int protocol;
    struct sockaddr_nl *addr;
};

static int netlink_socket_bind(struct resource *_this, const struct sockaddr *_addr, socklen_t addrlen) {
    struct netlink_socket *this = (void*)_this;
    const struct sockaddr_nl *addr = (void*)_addr;

    print("bind: Netlink socket\n");

    print("bind:    nl_family: %x\n", addr->nl_family);
    if (addr->nl_family != AF_NETLINK) {
        errno = EINVAL;
        return -1;
    }

    this->addr = addr;

    return 0;
}

struct resource *netlink_socket_new(int type, int protocol) {
    print("socket: requested Netlink socket, protocol: %x\n", protocol);

    struct netlink_socket *new_socket = resource_create(sizeof(struct netlink_socket));

    new_socket->bind = netlink_socket_bind;

    new_socket->protocol = protocol;

    return (void*)new_socket;
}
