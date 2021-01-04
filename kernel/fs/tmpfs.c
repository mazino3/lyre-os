#include <stddef.h>
#include <fs/vfs.h>
#include <fs/tmpfs.h>
#include <lib/alloc.h>
#include <lib/builtins.h>
#include <lib/handle.h>

struct tmpfs_file {
    size_t allocated_size;
    char  *data;
};

struct tmpfs_handle {
    struct handle;
    struct vfs_node *node;
    struct tmpfs_file *f;
};

static struct vfs_node *tmpfs_mount(struct handle *device) {
    (void)device;

    struct vfs_node *mount_gate = alloc(sizeof(struct vfs_node));

    mount_gate->fs = &tmpfs;

    return mount_gate;
}

static ssize_t tmpfs_read(struct tmpfs_handle *this, void *buf, size_t off, size_t count) {
    if (off + count > (size_t)this->node->st.st_size)
        count -= (off + count) - this->node->st.st_size;

    memcpy(buf, this->f->data + off, count);

    return count;
}

static ssize_t tmpfs_write(struct tmpfs_handle *this, const void *buf, size_t off, size_t count) {
    if (off + count > this->f->allocated_size) {
        while (off + count > this->f->allocated_size)
            this->f->allocated_size *= 2;

        this->f->data = realloc(this->f->data, this->f->allocated_size);
    }

    memcpy(this->f->data + off, buf, count);

    return count;
}


static struct handle *tmpfs_open(struct vfs_node *node, bool new_node) {
    struct tmpfs_handle *handle = handle_create(sizeof(struct tmpfs_handle));

    if (new_node) {
        struct tmpfs_file *tmpfs_file = alloc(sizeof(struct tmpfs_file));
        tmpfs_file->allocated_size = 4096;
        tmpfs_file->data = alloc(tmpfs_file->allocated_size);
        node->st.st_ino = (uintptr_t)tmpfs_file;
    }

    handle->f     = (void *)node->st.st_ino;
    handle->node  = node;
    handle->read  = (void *)tmpfs_read;
    handle->write = (void *)tmpfs_write;

    return (void *)handle;
}

struct filesystem tmpfs = {
    .name  = "tmpfs",
    .needs_backing_device = false,
    .mount = tmpfs_mount,
    .open  = tmpfs_open
};
