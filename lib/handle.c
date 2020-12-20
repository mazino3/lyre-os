#include <stddef.h>
#include <lib/handle.h>
#include <lib/types.h>
#include <lib/alloc.h>

// These functions should be stubs for generic kernel handles unused functions.

static ssize_t stub_read(struct handle *this, void *buf, size_t count) {
    (void)this;
    (void)buf;
    (void)count;
    return -1;
}

static ssize_t stub_write(struct handle *this, const void *buf, size_t count) {
    (void)this;
    (void)buf;
    (void)count;
    return -1;
}

void *handle_create(size_t actual_size) {
    struct handle *new_handle = alloc(actual_size);

    new_handle->actual_size = actual_size;

    new_handle->read  = stub_read;
    new_handle->write = stub_write;

    return new_handle;
}
