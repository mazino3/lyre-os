#ifndef __SOCKET__UNIX_H__
#define __SOCKET__UNIX_H__

#include <lib/resource.h>

struct resource *unix_socket_new(int type);

#endif
