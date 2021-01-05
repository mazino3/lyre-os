#ifndef __LIB__RESOURCE_H__
#define __LIB__RESOURCE_H__

#include <stddef.h>
#include <lib/types.h>
#include <lib/lock.h>

// This is the base class for all kernel handles.

struct resource {
    size_t actual_size;

    int    refcount;
    lock_t lock;

    struct stat st;

    int     (*close)(struct resource *this);
    ssize_t (*read)(struct resource *this, void *buf, off_t loc, size_t count);
    ssize_t (*write)(struct resource *this, const void *buf, off_t loc, size_t count);
    int     (*ioctl)(struct resource *this, int request, ...);
};

void *resource_create(size_t actual_size);

#endif
