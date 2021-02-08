#include <stddef.h>
#include <lib/resource.h>
#include <lib/types.h>
#include <lib/alloc.h>
#include <lib/ioctl.h>
#include <lib/errno.h>
#include <sched/sched.h>

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

static bool stub_mmap_hit(struct resource *this, struct mmap_range_local *range,
                          size_t memory_page, size_t file_page) {
    print("unhandled mmap_hit\n");
    return false;
}

static bool stub_mmap(struct resource *this, struct mmap_range_local *range) {
    print("unhandled mmap\n");
    return false;
}

static bool stub_munmap(struct resource *this, struct mmap_range_local *range) {
    print("unhandled munmap\n");
    return false;
}

void *resource_create(size_t actual_size) {
    struct resource *new = alloc(actual_size);

    new->actual_size = actual_size;

    new->close    = stub_close;
    new->read     = stub_read;
    new->write    = stub_write;
    new->ioctl    = stub_ioctl;
    new->bind     = stub_bind;
    new->mmap     = stub_mmap;
    new->munmap   = stub_munmap;
    new->mmap_hit = stub_mmap_hit;

    return new;
}

int fd_create_from_resource(struct vfs_node *dir, struct resource *res, int flags, int oldfd) {
    struct handle *handle = alloc(sizeof(struct handle));

    handle->res = res;
    handle->loc = 0;
    handle->flags = flags & FILE_STATUS_FLAGS_MASK;
    handle->refcount = 1;

    if (dir != NULL) {
        handle->is_directory = true;
        handle->node = dir;
    }

    struct file_descriptor *fd = alloc(sizeof(struct file_descriptor));

    fd->handle = handle;
    fd->flags = flags & FILE_DESCRIPTOR_FLAGS_MASK;

    return fd_create(fd, oldfd);
}

int fd_create(struct file_descriptor *fd, int oldfd) {
    struct process *process = this_cpu->current_thread->process;

    if (oldfd == -1) {
        return DYNARRAY_INSERT(process->fds, fd);
    } else {
        fd_close(oldfd);
        return DYNARRAY_INSERT_AT(process->fds, fd, (size_t)oldfd);
    }
}

int fd_create_least(struct file_descriptor *fd, int oldfd) {
    struct process *process = this_cpu->current_thread->process;

    return DYNARRAY_INSERT_LEAST(process->fds, fd, (size_t)oldfd);
}

struct file_descriptor *fd_from_fd(int fildes) {
    struct process *process = this_cpu->current_thread->process;
    if ((size_t)fildes >= process->fds.length) {
        errno = EBADF;
        return NULL;
    }
    return process->fds.storage[fildes];
}

struct handle *handle_from_fd(int fildes) {
    struct file_descriptor *fd = fd_from_fd(fildes);
    if (fd == NULL)
        return NULL;
    return fd->handle;
}

struct resource *resource_from_fd(int fildes) {
    struct handle *handle = handle_from_fd(fildes);
    if (handle == NULL)
        return NULL;
    return handle->res;
}

int fd_close(int fildes) {
    struct file_descriptor *fd = fd_from_fd(fildes);
    if (fd == NULL)
        return false;

    struct handle *handle = fd->handle;
    struct resource *res = handle->res;

    int ret = res->close(res);
    if (ret == -1)
        return false;

    if (--handle->refcount == 0)
        free(handle);

    free(fd);

    this_cpu->current_thread->process->fds.storage[fildes] = NULL;
    return 0;
}
