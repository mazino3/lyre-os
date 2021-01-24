#include <stddef.h>
#include <lib/resource.h>
#include <lib/types.h>
#include <lib/alloc.h>
#include <lib/ioctl.h>
#include <lib/errno.h>

// These functions should be stubs for generic kernel handles unused functions.

static int stub_close(struct resource *this) {
    (void)this;
    errno = EINVAL;
    return -1;
}

static ssize_t stub_read(struct resource *this, void *buf, off_t loc, size_t count) {
    (void)this;
    (void)buf;
    (void)loc;
    (void)count;
    errno = EINVAL;
    return -1;
}

static ssize_t stub_write(struct resource *this, const void *buf, off_t loc, size_t count) {
    (void)this;
    (void)buf;
    (void)loc;
    (void)count;
    errno = EINVAL;
    return -1;
}

static int stub_ioctl(struct resource *this, int request, void *argp) {
    (void)this;
    (void)request;
    (void)argp;
    switch (request) {
        case TCGETS: case TCSETS: case TIOCSCTTY: case TIOCGWINSZ:
            errno = ENOTTY;
            return -1;
    }
    errno = EINVAL;
    return -1;
}

static int stub_bind(struct resource *this, const struct sockaddr *addr, socklen_t addrlen) {
    (void)this;
    (void)addr;
    (void)addrlen;
    errno = ENOTSOCK;
    return -1;
}

void *resource_create(size_t actual_size) {
    struct resource *new = alloc(actual_size);

    new->actual_size = actual_size;

    new->close = stub_close;
    new->read  = stub_read;
    new->write = stub_write;
    new->ioctl = stub_ioctl;
    new->bind  = stub_bind;

    return new;
}
