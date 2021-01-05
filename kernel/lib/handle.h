#pragma once

#include <lib/types.h>

// This is the base class for all kernel handles.

struct handle {
    size_t actual_size;

    struct stat st;

    int     (*close)(struct handle *this);
    ssize_t (*read)(struct handle *this, void *buf, size_t count);
    ssize_t (*write)(struct handle *this, const void *buf, size_t count);
    int     (*ioctl)(struct handle *this, int request, ...);
};

void *handle_create(size_t actual_size);
