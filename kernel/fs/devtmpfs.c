#include <stddef.h>
#include <fs/vfs.h>
#include <fs/devtmpfs.h>
#include <lib/alloc.h>
#include <lib/builtins.h>
#include <lib/resource.h>
#include <lib/lock.h>

struct tmpfs_resource {
    struct resource;
    size_t allocated_size;
    char  *data;
};

static struct vfs_node devfs_mount_gate = {
    .name           = "/dev",
    .res            = NULL,
    .mount_data     = NULL,
    .fs             = &devtmpfs,
    .mount_gate     = NULL,
    .parent         = NULL,
    .child          = NULL,
    .next           = NULL,
    .backing_dev_id = 0
};

bool devtmpfs_add_device(struct resource *res, const char *name) {
    struct vfs_node *new_node = vfs_new_node_deep(&devfs_mount_gate, name);

    if (new_node == NULL)
        return false;

    new_node->res = res;

    return true;
}

static struct vfs_node *devtmpfs_mount(struct resource *device) {
    (void)device;

    return &devfs_mount_gate;
}

static ssize_t devtmpfs_read(struct resource *_this, void *buf, off_t off, size_t count) {
    struct tmpfs_resource *this = (void *)_this;

    if (!SPINLOCK_ACQUIRE(this->lock)) {
        return -1;
    }

    if (off + count > (size_t)this->st.st_size)
        count -= (off + count) - this->st.st_size;

    memcpy(buf, this->data + off, count);

    LOCK_RELEASE(this->lock);

    return count;
}

static ssize_t devtmpfs_write(struct resource *_this, const void *buf, off_t off, size_t count) {
    struct tmpfs_resource *this = (void *)_this;

    if (!SPINLOCK_ACQUIRE(this->lock)) {
        return -1;
    }

    if (off + count > this->allocated_size) {
        while (off + count > this->allocated_size)
            this->allocated_size *= 2;

        this->data = realloc(this->data, this->allocated_size);
    }

    memcpy(this->data + off, buf, count);

    this->st.st_size += count;

    LOCK_RELEASE(this->lock);

    return count;
}

static int devtmpfs_close(struct resource *_this) {
    struct tmpfs_resource *this = (void *)_this;

    if (!SPINLOCK_ACQUIRE(this->lock)) {
        return -1;
    }

    this->refcount--;

    LOCK_RELEASE(this->lock);

    return 0;
}

static struct resource *devtmpfs_open(struct vfs_node *node, bool create, mode_t mode) {
    if (!create)
        return NULL;

    struct tmpfs_resource *res = resource_create(sizeof(struct tmpfs_resource));

    res->allocated_size = 4096;
    res->data           = alloc(res->allocated_size);
    res->st.st_dev      = node->backing_dev_id;
    res->st.st_size     = 0;
    res->st.st_blocks   = 0;
    res->st.st_blksize  = 512;
    res->st.st_ino      = (uintptr_t)res->data;
    res->st.st_mode     = (mode & ~S_IFMT) | S_IFREG;
    res->st.st_nlink    = 1;
    res->close          = devtmpfs_close;
    res->read           = devtmpfs_read;
    res->write          = devtmpfs_write;

    return (void *)res;
}

static struct vfs_node *devtmpfs_populate(struct vfs_node *node) {
    (void)node;
    return NULL;
}

struct filesystem devtmpfs = {
    .name     = "devtmpfs",
    .needs_backing_device = false,
    .mount    = devtmpfs_mount,
    .open     = devtmpfs_open,
    .populate = devtmpfs_populate
};
