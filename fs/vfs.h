#ifndef __FS__VFS_H__
#define __FS__VFS_H__

#include <stdbool.h>
#include <lib/types.h>
#include <lib/handle.h>

struct filesystem {
    const char *name;
    bool needs_backing_device;
    struct vfs_node *(*mount)(struct handle *device);
    struct handle *(*open)(struct vfs_node *node, bool new_node);
};

struct vfs_node {
    char name[NAME_MAX];
    bool (*callback)(struct vfs_node *this);
    struct stat st;
    void *mount;
    struct filesystem *fs;
    struct vfs_node *mount_gate;
    struct vfs_node *child;
    struct vfs_node *next;
};

struct vfs_node *vfs_new_node(struct vfs_node *parent, const char *name);
void vfs_dump_nodes(struct vfs_node *node, const char *parent);
void vfs_get_absolute_path(char *path_ptr, const char *path, const char *pwd);
bool vfs_install_fs(struct filesystem *fs);
bool vfs_mount(const char *source, const char *target, const char *fs);
struct handle *vfs_open(const char *path, int oflags, mode_t mode);

#endif
