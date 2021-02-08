#ifndef __LIB__RESOURCE_H__
#define __LIB__RESOURCE_H__

#include <stddef.h>
#include <lib/types.h>
#include <lib/lock.h>
#include <socket/socket.h>

struct mmap_range_local;

struct resource {
    size_t actual_size;

    int    refcount;
    lock_t lock;

    struct stat st;

    int     (*close)(struct resource *this);
    ssize_t (*read)(struct resource *this, void *buf, off_t loc, size_t count);
    ssize_t (*write)(struct resource *this, const void *buf, off_t loc, size_t count);
    int     (*ioctl)(struct resource *this, int request, void *argp);

    int     (*bind)(struct resource *this, const struct sockaddr *addr, socklen_t addrlen);

    bool (*mmap)(struct resource *this, struct mmap_range_local *range);
    bool (*munmap)(struct resource *this, struct mmap_range_local *range);
    bool (*mmap_hit)(struct resource *this, struct mmap_range_local *range,
                     size_t memory_page, size_t file_page);
};

struct vfs_node;

struct handle {
    int is_directory;
    struct resource *res;
    struct vfs_node *node;
    struct vfs_node *cur_dirent;
    int refcount;
    off_t loc;
    int flags;
};

struct file_descriptor {
    struct handle *handle;
    int flags;
};

void *resource_create(size_t actual_size);
int fd_create(struct file_descriptor *fd, int oldfd);
int fd_create_from_resource(struct vfs_node *dir, struct resource *res, int flags, int oldfd);
int fd_create_least(struct file_descriptor *fd, int oldfd);
int fd_close(int fildes);
struct file_descriptor *fd_from_fd(int fildes);
struct handle *handle_from_fd(int fildes);
struct resource *resource_from_fd(int fildes);

#define FILE_CREATION_FLAGS_MASK ( \
    O_CREAT | O_DIRECTORY | O_EXCL | O_NOCTTY | O_NOFOLLOW | O_TRUNC \
)

#define FILE_DESCRIPTOR_FLAGS_MASK ( \
    O_CLOEXEC \
)

#define FILE_STATUS_FLAGS_MASK ( \
    ~(FILE_CREATION_FLAGS_MASK | FILE_DESCRIPTOR_FLAGS_MASK) \
)

#endif
