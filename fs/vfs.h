#ifndef __FS__VFS_H__
#define __FS__VFS_H__

#include <stdbool.h>
#include <lib/types.h>
#include <lib/handle.h>

enum vfs_node_event {
    VFS_NODE_NO_EVENT,
    VFS_NODE_POPULATE_EVENT
};

struct filesystem {
    char name[256];
    struct vfs_node *(*mount)(struct handle *device);
    struct vfs_node *(*populate)(void *mount, struct stat *st);
    struct handle *(*open)(void *mount, struct stat *st);
};

struct vfs_node {
    char name[256];
    enum vfs_node_event event_pending;
    struct stat st;
    void *mount;
    struct filesystem *fs;
    struct vfs_node *mount_gate;
    struct vfs_node *child;
    struct vfs_node *next;
};

void vfs_get_absolute_path(char *path_ptr, const char *path, const char *pwd);
bool vfs_install_fs(struct filesystem *fs);
bool vfs_mount(const char *source, const char *target, const char *fs);
struct handle *vfs_open(const char *path, int oflags, ...);

#endif
