#include <stddef.h>
#include <stdbool.h>
#include <fs/vfs.h>
#include <lib/dynarray.h>
#include <lib/print.h>

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

static struct vfs_node root_node = {
    .name          = "/",
    .st            = { .st_mode = S_IFDIR },
    .callback      = NULL,
    .mount         = NULL,
    .fs            = NULL,
    .mount_gate    = NULL,
    .child         = NULL,
    .next          = NULL
};

static struct vfs_node *path2node(const char *_path, bool *create) {
    char abs_path[strlen(_path) + 2];
    bool last = false;

    vfs_get_absolute_path(abs_path, _path, "/");

    char *path = abs_path + 1;

    if (*path == 0) {
        if (create != NULL)
            *create = false;
        return &root_node;
    }

    if (root_node.mount_gate == NULL)
        return NULL;

    struct vfs_node *cur_parent = NULL;
    struct vfs_node *cur_node   = NULL;

next:;
    char *elem = path;
    while (*path != 0 && *path != '/')
        path++;
    if (*path == '/')
        *path++ = 0;
    else /* path == 0 */
        last = true;

    if (cur_parent == NULL)
        cur_parent = root_node.mount_gate;
    if (cur_node == NULL)
        cur_node = cur_parent->child;
    if (cur_node == NULL)
        goto epilogue;

    for (;;) {
        if (strcmp(cur_node->name, elem)) {
            if (cur_node->next == NULL)
                break;
            cur_node = cur_node->next;
            continue;
        }

        if (last) {
            if (create != NULL)
                *create = false;
            return cur_node;
        }

        if (!S_ISDIR(cur_node->st.st_mode) || !S_ISLNK(cur_node->st.st_mode)) {
            // errno = ENOTDIR;
            return NULL;
        }

        if (cur_node->mount_gate != NULL)
            cur_node = cur_node->mount_gate;

        if (cur_node->callback != NULL && !cur_node->callback(cur_node))
            return NULL;

        if (cur_node->child == NULL)
            return NULL;

        cur_parent = cur_node;
        cur_node   = cur_node->child;
        goto next;
    }

epilogue:
    if (create != NULL && *create) {
        if (last)
            return vfs_new_node(cur_parent, elem);
        else
            *create = false;
    }

    // if (last)
    //     errno = ENOENT;
    // else
    //     errno = ENOTDIR;
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

    struct vfs_node *tgt_node = path2node(target, NULL);
    if (tgt_node == NULL)
        return false;

    if (!S_ISDIR(tgt_node->st.st_mode)) {
        // errno = ENOTDIR;
        return false;
    }

    struct handle *src_handle = NULL;
    if (fs->needs_backing_device) {
        struct vfs_node *backing_dev_node = path2node(source, NULL);
        if (backing_dev_node == NULL)
            return false;
        if (!S_ISCHR(backing_dev_node->st.st_mode) && !S_ISBLK(backing_dev_node->st.st_mode))
            return false;
        struct handle *src_handle = vfs_open(source, O_RDWR, 0);
        if (src_handle == NULL)
            return false;
    }

    struct vfs_node *mount_gate = fs->mount(src_handle);
    if (mount_gate == NULL) {
        // vfs_close(src_handle);
        return false;
    }

    tgt_node->mount_gate = mount_gate;

    print("vfs: Mounted `%s` on `%s`, type: `%s`.\n", source, target, fstype);

    return true;
}

struct vfs_node *vfs_new_node(struct vfs_node *parent, const char *name) {
    struct vfs_node *new_node = alloc(sizeof(struct vfs_node));

    new_node->next = parent->child;
    parent->child  = new_node;

    strcpy(new_node->name, name);
    new_node->mount = parent->mount;
    new_node->fs    = parent->fs;

    return new_node;
}

struct handle *vfs_open(const char *path, int oflags, mode_t mode) {
    (void)oflags;

    bool create = oflags & O_CREAT;
    struct vfs_node *path_node = path2node(path, &create);
    if (path_node == NULL)
        return NULL;

    if (create)
        path_node->st.st_mode = mode;

    struct handle *handle = path_node->fs->open(path_node, create);

    return handle;
}

void vfs_dump_nodes(struct vfs_node *node, const char *parent) {
    struct vfs_node *cur_node = node ? node : &root_node;
    while (cur_node) {
        print("%s - %s\n", parent, cur_node->name);
        if (cur_node->mount_gate != NULL && cur_node->mount_gate->child != NULL)
            vfs_dump_nodes(cur_node->mount_gate->child, cur_node->name);
        else if (cur_node->child != NULL)
            vfs_dump_nodes(cur_node->child, cur_node->name);
        cur_node = cur_node->next;
    }
}
