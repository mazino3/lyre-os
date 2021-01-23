#include <stddef.h>
#include <stdbool.h>
#include <fs/vfs.h>
#include <lib/dynarray.h>
#include <lib/print.h>
#include <dev/dev.h>
#include <lib/lock.h>
#include <sys/cpu.h>
#include <sched/sched.h>
#include <lib/errno.h>

#define AT_FDCWD -100

lock_t vfs_lock = {0};

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

static struct resource root_res = {
    .st = {
        .st_mode = S_IFDIR,
        .st_ino  = VFS_ROOT_INODE
    }
};

struct vfs_node vfs_root_node = {
    .name           = "/",
    .res            = &root_res,
    .mount_data     = NULL,
    .fs             = NULL,
    .mount_gate     = NULL,
    .parent         = NULL,
    .child          = NULL,
    .next           = NULL,
    .backing_dev_id = 0
};

enum {
    NO_CREATE = 0,
    CREATE_SHALLOW,
    CREATE_DEEP
};

static struct vfs_node *path2node(struct vfs_node *parent, const char *_path, int create) {
    bool last = false;

    if (_path == NULL)
        return NULL;

    if (*_path == 0)
        return NULL;

    char l_path[strlen(_path) + 1];
    strcpy(l_path, _path);

    char *path = l_path;

    // Get rid of trailing slashes
    for (ssize_t i = strlen(path) - 1; i > 0; i--) {
        if (path[i] == '/')
            path[i] = 0;
        else
            break;
    }

    struct vfs_node *cur_parent = *path == '/' || parent == NULL ? &vfs_root_node : parent;
    if (cur_parent->mount_gate)
        cur_parent = cur_parent->mount_gate;
    struct vfs_node *cur_node   = cur_parent->child;

    while (*path == '/') {
        path++;
        if (!*path)
            return &vfs_root_node;
    }

next:;
    char *elem = path;
    while (*path != 0) {
        if (*path == '/') {
            if (*(path + 1) == '/') {
                path++;
                continue;
            } else {
                break;
            }
        }
        path++;
    }
    if (*path == '/')
        *path++ = 0;
    else /* path == 0 */
        last = true;

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
            return cur_node;
        }

        if (!S_ISDIR(cur_node->res->st.st_mode)) {
            errno = ENOTDIR;
            return NULL;
        }

        if (cur_node->mount_gate != NULL)
            cur_node = cur_node->mount_gate;

        if (cur_node->child == NULL) {
            cur_node->child = cur_node->fs->populate(cur_node);
            if (cur_node->child == NULL) {
                errno = ENOTDIR;
                return NULL;
            }
        }

        cur_parent = cur_node;
        cur_node   = cur_node->child;
        goto next;
    }

epilogue:
    if (create) {
        if (last) {
            return vfs_new_node(cur_parent, elem);
        } else {
            if (create == CREATE_SHALLOW)
                return NULL;
            cur_parent = vfs_mkdir(cur_parent, elem, 0755, false);
            goto next;
        }
    }

    if (last)
        errno = ENOENT;
    else
        errno = ENOTDIR;
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

    struct vfs_node *tgt_node = path2node(NULL, target, NO_CREATE);
    if (tgt_node == NULL)
        return false;

    if (!S_ISDIR(tgt_node->res->st.st_mode)) {
        errno = ENOTDIR;
        return false;
    }

    dev_t backing_dev_id;
    struct resource *src_handle = NULL;
    if (fs->needs_backing_device == BACKING_DEV_YES) {
        struct vfs_node *backing_dev_node = path2node(NULL, source, NO_CREATE);
        if (backing_dev_node == NULL)
            return false;
        if (!S_ISCHR(backing_dev_node->res->st.st_mode)
         && !S_ISBLK(backing_dev_node->res->st.st_mode))
            return false;
        struct resource *src_handle = vfs_open(source, O_RDWR, 0);
        if (src_handle == NULL)
            return false;
        backing_dev_id = backing_dev_node->res->st.st_rdev;
    } else if (fs->needs_backing_device == BACKING_DEV_NO) {
        backing_dev_id = dev_new_id();
    }

    struct vfs_node *mount_gate = fs->mount(src_handle);
    if (mount_gate == NULL) {
        src_handle->close(src_handle);
        return false;
    }

    if (fs->needs_backing_device != BACKING_DEV_NO_NOGEN) {
        mount_gate->backing_dev_id = backing_dev_id;
    }

    tgt_node->mount_gate = mount_gate;

    print("vfs: Mounted `%s` on `%s`, type: `%s`.\n", source, target, fstype);

    return true;
}

struct vfs_node *vfs_mkdir(struct vfs_node *parent, const char *name, mode_t mode, bool recurse) {
    if (parent == NULL)
        parent = &vfs_root_node;

    struct vfs_node *new_dir = path2node(parent, name, NO_CREATE);

    if (new_dir != NULL)
        return NULL;

    new_dir = path2node(parent, name, recurse ? CREATE_DEEP : CREATE_SHALLOW);

    if (new_dir == NULL)
        return NULL;

    new_dir->res = new_dir->fs->mkdir(new_dir, mode);

    struct vfs_node *dot = vfs_new_node(new_dir, ".");
    dot->child = new_dir;
    dot->res   = new_dir->res;

    struct vfs_node *dotdot = vfs_new_node(new_dir, "..");
    dotdot->child = parent;
    dotdot->res   = parent->res;

    return new_dir;
}

struct vfs_node *vfs_new_node(struct vfs_node *parent, const char *name) {
    if (parent == NULL)
        parent = &vfs_root_node;

    if (parent->mount_gate)
        parent = parent->mount_gate;

    struct vfs_node *new_node = path2node(parent, name, NO_CREATE);

    if (new_node != NULL)
        return NULL;

    new_node = alloc(sizeof(struct vfs_node));

    new_node->next = parent->child;
    parent->child  = new_node;

    strcpy(new_node->name, name);
    new_node->fs             = parent->fs;
    new_node->mount_data     = parent->mount_data;
    new_node->backing_dev_id = parent->backing_dev_id;
    new_node->parent         = parent;

    return new_node;
}

struct vfs_node *vfs_new_node_deep(struct vfs_node *parent, const char *name) {
    struct vfs_node *new_node = path2node(parent, name, NO_CREATE);

    if (new_node != NULL)
        return NULL;

    new_node = path2node(parent, name, CREATE_DEEP);

    return new_node;
}

void syscall_open(struct cpu_gpr_context *ctx) {
    const char *path  = (const char *) ctx->rdi;
    int         flags = (int)          ctx->rsi;
    mode_t      mode  = (mode_t)       ctx->rdx;

    struct resource *res = vfs_open(path, flags, mode);

    if (res == NULL) {
        ctx->rax = (uint64_t)-1;
        return;
    }

    struct handle *handle = alloc(sizeof(struct handle));

    handle->res = res;
    handle->loc = 0;

    struct process *process = this_cpu->current_thread->process;

    int ret = DYNARRAY_INSERT(process->handles, handle);

    ctx->rax = (uint64_t)ret;
}

void syscall_close(struct cpu_gpr_context *ctx) {
    int fd = (int)ctx->rdi;

    struct process *process = this_cpu->current_thread->process;

    struct handle *handle = process->handles.storage[fd];

    ctx->rax = (uint64_t)handle->res->close(handle->res);

    process->handles.storage[fd] = NULL;
    free(handle);
}

void syscall_read(struct cpu_gpr_context *ctx) {
    int    fd    = (int)    ctx->rdi;
    void  *buf   = (void *) ctx->rsi;
    size_t count = (size_t) ctx->rdx;

    struct process *process = this_cpu->current_thread->process;

    struct handle *handle = process->handles.storage[fd];

    ssize_t ret = handle->res->read(handle->res, buf, handle->loc, count);

    if (ret == -1) {
        ctx->rax = (uint64_t)-1;
        return;
    }

    handle->loc += ret;

    ctx->rax = (uint64_t)ret;
}

void syscall_write(struct cpu_gpr_context *ctx) {
    int    fd    = (int)    ctx->rdi;
    void  *buf   = (void *) ctx->rsi;
    size_t count = (size_t) ctx->rdx;

    struct process *process = this_cpu->current_thread->process;

    struct handle *handle = process->handles.storage[fd];

    ssize_t ret = handle->res->write(handle->res, buf, handle->loc, count);

    if (ret == -1) {
        ctx->rax = (uint64_t)-1;
        return;
    }

    handle->loc += ret;

    ctx->rax = (uint64_t)ret;
}

void syscall_ioctl(struct cpu_gpr_context *ctx) {
    int   fd      = (int)    ctx->rdi;
    int   request = (int)    ctx->rsi;
    void *argp    = (void *) ctx->rdx;

    struct process *process = this_cpu->current_thread->process;

    struct handle *handle = process->handles.storage[fd];

    int ret = handle->res->ioctl(handle->res, request, argp);

    if (ret == -1) {
        ctx->rax = (uint64_t)-1;
        return;
    }

    ctx->rax = (uint64_t)ret;
}

void syscall_chdir(struct cpu_gpr_context *ctx) {
    const char *path = (const char *)ctx->rdi;

    struct process *process = this_cpu->current_thread->process;

    struct vfs_node *new_dir = path2node(process->current_directory, path, NO_CREATE);

    if (new_dir == NULL) {
        ctx->rax = (uint64_t)-1;
        return;
    }

    if (!S_ISDIR(new_dir->res->st.st_mode)) {
        ctx->rax = (uint64_t)-1;
        errno = ENOTDIR;
        return;
    }

    process->current_directory = new_dir;
    ctx->rax = 0;
}

void syscall_mkdirat(struct cpu_gpr_context *ctx) {
    int         dirfd = (int)          ctx->rdi;
    const char *path  = (const char *) ctx->rsi;
    mode_t      mode  = (mode_t)       ctx->rdx;

    bool is_absolute = *path == '/';

    struct process *process = this_cpu->current_thread->process;

    struct vfs_node *parent;
    if (is_absolute) {
        parent = &vfs_root_node;
    } else {
        if (dirfd == AT_FDCWD) {
            parent = process->current_directory;
        } else {
            struct handle *dir_handle = process->handles.storage[dirfd];

            if (dir_handle->type != HANDLE_DIRECTORY) {
                errno = ENOTDIR;
                ctx->rax = (uint64_t)-1;
                return;
            }

            parent = dir_handle->node;
        }
    }

    struct vfs_node *new_dir = vfs_mkdir(parent, path, mode, false);

    if (new_dir == NULL) {
        ctx->rax = (uint64_t)-1;
        return;
    }

    ctx->rax = 0;
}

#define SEEK_CUR 1
#define SEEK_END 2
#define SEEK_SET 3

void syscall_seek(struct cpu_gpr_context *ctx) {
    int   fd     = (int)   ctx->rdi;
    off_t offset = (off_t) ctx->rsi;
    int   whence = (int)   ctx->rdx;

    struct process *process = this_cpu->current_thread->process;

    struct handle *handle = process->handles.storage[fd];

    if (handle->res->st.st_size == 0) {
        ctx->rax = 0;
        return;
    }

    off_t base;
    switch (whence) {
        case SEEK_SET:
            base = offset;
            break;
        case SEEK_CUR:
            base = handle->loc + offset;
            break;
        case SEEK_END:
            base = handle->res->st.st_size + offset;
            break;
        default:
            errno = EINVAL;
            ctx->rax = (uint64_t)-1;
            return;
    }

    if (base < 0 || base >= handle->res->st.st_size) {
        errno = EINVAL;
        ctx->rax = (uint64_t)-1;
        return;
    }

    handle->loc = base;
    ctx->rax = (uint64_t)base;
}

struct resource *vfs_open(const char *path, int oflags, mode_t mode) {
    SPINLOCK_ACQUIRE(vfs_lock);

    bool create = oflags & O_CREAT;
    struct vfs_node *path_node = path2node(NULL, path, create ? CREATE_SHALLOW : NO_CREATE);
    if (path_node == NULL) {
        LOCK_RELEASE(vfs_lock);
        return NULL;
    }

    if (path_node->res == NULL)
        path_node->res = path_node->fs->open(path_node, create, mode);

    if (path_node->res == NULL) {
        LOCK_RELEASE(vfs_lock);
        return NULL;
    }

    struct resource *res = path_node->res;

    SPINLOCK_ACQUIRE(res->lock);
    res->refcount++;
    LOCK_RELEASE(res->lock);

    LOCK_RELEASE(vfs_lock);

    return res;
}

void vfs_dump_nodes(struct vfs_node *node, const char *parent) {
    struct vfs_node *cur_node = node ? node : &vfs_root_node;
    for (; cur_node; cur_node = cur_node->next) {
        print("%s - %s\n", parent, cur_node->name);
        if (!strcmp(cur_node->name, ".") || !strcmp(cur_node->name, ".."))
            continue;
        if (cur_node->mount_gate != NULL && cur_node->mount_gate->child != NULL) {
            vfs_dump_nodes(cur_node->mount_gate->child, cur_node->name);
        } else if (cur_node->child != NULL && cur_node->mount_gate == NULL) {
            vfs_dump_nodes(cur_node->child, cur_node->name);
        }
    }
}

bool vfs_stat(const char *path, struct stat *st) {
    SPINLOCK_ACQUIRE(vfs_lock);

    struct vfs_node *node = path2node(NULL, path, NO_CREATE);
    if (node == NULL) {
        errno = ENOENT;
        LOCK_RELEASE(vfs_lock);
        return false;
    }

    *st = node->res->st;

    LOCK_RELEASE(vfs_lock);
    return true;
}
