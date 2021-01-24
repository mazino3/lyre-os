#ifndef __SOCKET__NETLINK_H__
#define __SOCKET__NETLINK_H__

#include <socket/socket.h>
#include <lib/resource.h>

#define NETLINK_ROUTE 0
#define NETLINK_USERSOCK 2
#define NETLINK_FIREWALL 3
#define NETLINK_IP6_FW 13
#define NETLINK_KOBJECT_UEVENT 15

struct sockaddr_nl {
	sa_family_t nl_family;
	unsigned short nl_pad;
	unsigned int nl_pid;
	unsigned int nl_groups;
};

struct resource *netlink_socket_new(int type, int family);

#endif
