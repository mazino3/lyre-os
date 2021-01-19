#ifndef __FS__DEVTMPFS_H__
#define __FS__DEVTMPFS_H__

#include <stdbool.h>
#include <fs/vfs.h>
#include <lib/resource.h>

extern struct filesystem devtmpfs;

bool devtmpfs_add_device(struct resource *res, const char *name);

#endif
