#include <stdbool.h>
#include <fs/vfs.h>
#include <lib/dynarray.h>

DYNARRAY_STATIC(struct filesystem *, filesystems);

bool vfs_install_fs(struct filesystem *fs) {
    DYNARRAY_PUSHBACK(filesystems, fs);
    return true;
}

/* Convert a relative path into an absolute path.
   This is a freestanding function and can be used for any purpose :) */
void vfs_get_absolute_path(char *path_ptr, const char *path, const char *pwd) {
    char *orig_ptr = path_ptr;

    if (!*path) {
        strcpy(path_ptr, pwd);
        return;
    }

    if (*path != '/') {
        strcpy(path_ptr, pwd);
        path_ptr += strlen(path_ptr);
    } else {
        *path_ptr = '/';
        path_ptr++;
        path++;
    }

    goto first_run;

    for (;;) {
        switch (*path) {
            case '/':
                path++;
first_run:
                if (*path == '/') continue;
                if ((!strncmp(path, ".\0", 2))
                ||  (!strncmp(path, "./\0", 3))) {
                    goto term;
                }
                if ((!strncmp(path, "..\0", 3))
                ||  (!strncmp(path, "../\0", 4))) {
                    while (*path_ptr != '/') path_ptr--;
                    if (path_ptr == orig_ptr) path_ptr++;
                    goto term;
                }
                if (!strncmp(path, "../", 3)) {
                    while (*path_ptr != '/') path_ptr--;
                    if (path_ptr == orig_ptr) path_ptr++;
                    path += 2;
                    *path_ptr = 0;
                    continue;
                }
                if (!strncmp(path, "./", 2)) {
                    path += 1;
                    continue;
                }
                if (((path_ptr - 1) != orig_ptr) && (*(path_ptr - 1) != '/')) {
                    *path_ptr = '/';
                    path_ptr++;
                }
                continue;
            case '\0':
term:
                if ((*(path_ptr - 1) == '/') && ((path_ptr - 1) != orig_ptr))
                    path_ptr--;
                *path_ptr = 0;
                return;
            default:
                *path_ptr = *path;
                path++;
                path_ptr++;
                continue;
        }
    }
}

static bool do_events(struct vfs_node *node) {
    switch (node->event_pending) {
        case VFS_NODE_NO_EVENT:
            return true;
        case VFS_NODE_POPULATE_EVENT: {
            struct vfs_node *subdir;
            subdir = node->fs->populate(node->mount, &node->st);
            if (!subdir)
                return false;
            node->child = subdir;
            return true;
        }
    }
}

static struct vfs_node vfs_root = {
    .name          = "/",
    .st            = { .st_mode = S_IFDIR },
    .event_pending = VFS_NODE_NO_EVENT,
    .mount         = NULL,
    .fs            = NULL,
    .mount_gate    = NULL,
    .child         = NULL,
    .next          = NULL
};

static struct vfs_node *path2node(const char *_path) {
    struct vfs_node *cur_node = &vfs_root;
    char abs_path[strlen(_path) + 2];
    bool last = false;

    vfs_get_absolute_path(abs_path, _path, "/");

    char *path = abs_path + 1;

    if (*path == 0)
        return &vfs_root;

next:;
    char *elem = path;
    while (*path != 0 && *path != '/')
        path++;
    if (*path == '/')
        *path++ = 0;
    else /* path == 0 */
        last = true;

    while (cur_node->next != NULL) {
        if (strcmp(cur_node->name, elem)) {
            cur_node = cur_node->next;
            continue;
        }

        if (last)
            return cur_node;

        if (!S_ISDIR(cur_node->st.st_mode) || !S_ISLNK(cur_node->st.st_mode)) {
            // errno = ENOTDIR;
            return NULL;
        }

        if (cur_node->mount_gate != NULL) {
            cur_node = cur_node->mount_gate;
            continue;
        }

        if (!do_events(cur_node))
            return NULL;

        cur_node = cur_node->child;
        goto next;
    }

    // errno = ENOENT;
    return NULL;
}

static struct filesystem *fstype2fs(const char *fstype) {
    for (size_t i = 0; i < filesystems.length; i++) {
        if (!strcmp(filesystems.storage[i]->name, fstype))
            return filesystems.storage[i];
    }

    return NULL;
}

bool vfs_mount(const char *source, const char *target, const char *fstype) {
    struct filesystem *fs = fstype2fs(fstype);
    if (fs == NULL)
        return false;

    struct vfs_node *tgt_node;
    if (!strcmp(target, "/")) {
        tgt_node = &vfs_root;
    } else {
        tgt_node = path2node(target);
        if (tgt_node == NULL)
            return false;
    }

    if (!S_ISDIR(tgt_node->st.st_mode)) {
        // errno = ENOTDIR;
        return false;
    }

    struct handle *src_handle = vfs_open(source, O_RDWR);
    if (src_handle == NULL)
        return false;

    struct vfs_node *mount_gate = fs->mount(src_handle);
    if (mount_gate == NULL) {
        // vfs_close(src_handle);
        return false;
    }

    tgt_node->mount_gate = mount_gate;

    return true;
}

struct handle *vfs_open(const char *path, int oflags, ...) {
    (void)oflags;

    struct vfs_node *path_node = path2node(path);
    if (path_node == NULL)
        return NULL;

    struct handle *handle = path_node->fs->open(path_node->mount, &path_node->st);

    return handle;
}
