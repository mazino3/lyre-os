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

struct vfs_node *vfs_root_node;

#define NO_CREATE      0b0000
#define CREATE_SHALLOW 0b0001
#define CREATE_DEEP    0b0010
#define NO_DEREF_LINKS 0b0100
#define FAIL_IF_EXISTS 0b1000

static struct vfs_node *path2node(struct vfs_node *parent, const char *_path, int flags) {
    bool last = false;

    if (_path == NULL)
        return NULL;

    if (*_path == 0)
        return NULL;

    char l_path[strlen(_path) + 1];
    strcpy(l_path, _path);

    char *path = l_path;

    struct vfs_node *cur_parent = *path == '/' || parent == NULL ? vfs_root_node : parent;
    if (cur_parent->mount_gate)
        cur_parent = cur_parent->mount_gate;

    struct vfs_node *cur_node = cur_parent->child;

    while (*path == '/')
        path++;
    if (*path == 0)
        return vfs_root_node;

next:;
    char *elem = path;
    while (*path != 0 && *path != '/')
        path++;
    if (*path == '/') {
        *path++ = 0;
        while (*path == '/')
            path++;
    }
    if (*path == 0)
        last = true;

    if (cur_node == NULL)
        goto epilogue;

    for (;;) {
        bool mount_peek = false;
        struct vfs_node *mount_parent;

        if (cur_node->mount_gate != NULL) {
            mount_peek = true;
            mount_parent = cur_node;
            cur_node = cur_node->mount_gate;
        }

        if (strcmp(cur_node->name, elem)) {
            if (mount_peek)
                cur_node = mount_parent->next;
            else
                cur_node = cur_node->next;
            if (cur_node == NULL)
                break;
            continue;
        }

        if (last) {
            if (flags & FAIL_IF_EXISTS) {
                errno = EEXIST;
                return NULL;
            }

            if (cur_node->res != NULL && S_ISLNK(cur_node->res->st.st_mode)) {
                if (flags & NO_DEREF_LINKS) {
                    errno = ELOOP;
                    return NULL;
                }

                return path2node(cur_node->parent, cur_node->target, flags);
            }

            return cur_node;
        }

        if (cur_node->res == NULL || !S_ISDIR(cur_node->res->st.st_mode)) {
            errno = ENOTDIR;
            return NULL;
        }

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
    if (flags & (CREATE_SHALLOW | CREATE_DEEP)) {
        if (last) {
            return vfs_new_node(cur_parent, elem);
        } else {
            if (!(flags & CREATE_DEEP)) {
                return NULL;
            }
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

    bool mounting_root = false;
    for (size_t i = 0; target[i] == '/'; i++) {
        if (target[i+1] == 0) {
            mounting_root = true;
            break;
        }
    }

    struct vfs_node *tgt_node;
    if (!mounting_root) {
        tgt_node = path2node(NULL, target, NO_CREATE);
        if (tgt_node == NULL)
            return false;

        if (!S_ISDIR(tgt_node->res->st.st_mode)) {
            errno = ENOTDIR;
            return false;
        }
    }

    dev_t backing_dev_id = 0;
    struct resource *src_handle = NULL;
    if (fs->needs_backing_device == BACKING_DEV_YES) {
        struct vfs_node *backing_dev_node = path2node(NULL, source, NO_CREATE);
        if (backing_dev_node == NULL)
            return false;
        if (!S_ISCHR(backing_dev_node->res->st.st_mode)
         && !S_ISBLK(backing_dev_node->res->st.st_mode))
            return false;
        struct resource *src_handle = vfs_open(NULL, NULL, source, O_RDWR, 0);
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
        mount_gate->res->st.st_dev = backing_dev_id;
    }

    if (mounting_root) {
        vfs_root_node = mount_gate;
        mount_gate->parent = NULL;
        strcpy(mount_gate->name, "/");

        struct vfs_node *dot = vfs_new_node(mount_gate, ".");
        dot->child = mount_gate;
        dot->res   = mount_gate->res;

        struct vfs_node *dotdot = vfs_new_node(mount_gate, "..");
        dotdot->child = mount_gate;
        dotdot->res   = mount_gate->res;
    } else {
        tgt_node->mount_gate = mount_gate;
        mount_gate->parent = tgt_node->parent;
        strcpy(mount_gate->name, tgt_node->name);

        struct vfs_node *dot = vfs_new_node(mount_gate, ".");
        dot->child = mount_gate;
        dot->res   = mount_gate->res;

        struct vfs_node *dotdot = vfs_new_node(mount_gate, "..");
        dotdot->child = mount_gate->parent;
        dotdot->res   = mount_gate->parent->res;
    }

    print("vfs: Mounted `%s` on `%s`, type: `%s`.\n", source, target, fstype);

    return true;
}

struct vfs_node *vfs_mkdir(struct vfs_node *parent, const char *name, mode_t mode, bool recurse) {
    if (parent == NULL)
        parent = vfs_root_node;

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
        parent = vfs_root_node;

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

static struct vfs_node *get_parent_dir(int dirfd, const char *path) {
    bool is_absolute = *path == '/';

    struct process *process = this_cpu->current_thread->process;

    struct vfs_node *parent;
    if (is_absolute) {
        parent = vfs_root_node;
    } else {
        if (dirfd == AT_FDCWD) {
            parent = process->current_directory;
        } else {
            struct handle *dir_handle = handle_from_fd(dirfd);
            if (dir_handle == NULL)
                return NULL;

            if (!dir_handle->is_directory) {
                errno = ENOTDIR;
                return NULL;
            }

            parent = dir_handle->node;
        }
    }

    return parent;
}

// fcntl constants from mlibc follow...

// constants for fcntl()'s command argument
#define F_DUPFD 1
#define F_DUPFD_CLOEXEC 2
#define F_GETFD 3
#define F_SETFD 4
#define F_GETFL 5
#define F_SETFL 6
#define F_GETLK 7
#define F_SETLK 8
#define F_SETLKW 9
#define F_GETOWN 10
#define F_SETOWN 11

// constants for struct flock's l_type member
#define F_RDLCK 1
#define F_UNLCK 2
#define F_WRLCK 3

// constants for fcntl()'s additional argument of F_GETFD and F_SETFD
#define FD_CLOEXEC 1

static int dup(int oldfd, int least) {
    struct file_descriptor *old_fd = fd_from_fd(oldfd);
    if (old_fd == NULL) {
        return -1;
    }

    struct file_descriptor *new_fd = alloc(sizeof(struct file_descriptor));
    *new_fd = *old_fd;
    return fd_create_least(new_fd, least);
}

void syscall_fcntl(struct cpu_gpr_context *ctx) {
    int fildes = (int) ctx->rdi;
    int cmd    = (int) ctx->rsi;

    struct file_descriptor *fd = fd_from_fd(fildes);
    if (fd == NULL) {
        ctx->rax = (uint64_t)-1;
        return;
    }

    struct handle *handle = fd->handle;

    switch (cmd) {
        case F_DUPFD:
            ctx->rax = (uint64_t)dup(fildes, (int)ctx->rdx);
            return;
        case F_GETFD:
            ctx->rax = (uint64_t)((fd->flags & O_CLOEXEC) ? FD_CLOEXEC : 0);
            return;
        case F_SETFD:
            fd->flags = (int)((ctx->rdx & FD_CLOEXEC) ? O_CLOEXEC : 0);
            ctx->rax = (uint64_t)0;
            return;
        case F_GETFL:
            ctx->rax = (uint64_t)handle->flags;
            return;
        case F_SETFL:
            handle->flags = (int)ctx->rdx;
            ctx->rax = (uint64_t)0;
            return;
        default:
            print("\nfcntl: Unhandled command: %d\n", cmd);
            errno = EINVAL;
            ctx->rax = (uint64_t)-1;
            return;
    }
}

void syscall_dup3(struct cpu_gpr_context *ctx) {
    int oldfd = (int) ctx->rdi;
    int newfd = (int) ctx->rsi;
    int flags = (int) ctx->rdx;

    if (oldfd == newfd) {
        errno = EINVAL;
        ctx->rax = (uint64_t)-1;
        return;
    }

    struct file_descriptor *old_fd = fd_from_fd(oldfd);
    if (old_fd == NULL) {
        ctx->rax = (uint64_t)-1;
        return;
    }

    struct file_descriptor *new_fd = alloc(sizeof(struct file_descriptor));
    *new_fd = *old_fd;
    fd_create(new_fd, newfd);

    new_fd->flags = flags & FILE_DESCRIPTOR_FLAGS_MASK;

    ctx->rax = (uint64_t)0;
}

void syscall_openat(struct cpu_gpr_context *ctx) {
    int         dirfd = (int)          ctx->rdi;
    const char *path  = (const char *) ctx->rsi;
    int         flags = (int)          ctx->rdx;
    mode_t      mode  = (mode_t)       ctx->r10;

    struct vfs_node *parent = get_parent_dir(dirfd, path);
    if (parent == NULL) {
        ctx->rax = (uint64_t)-1;
        return;
    }

    int creat_flags = flags & FILE_CREATION_FLAGS_MASK;

    struct vfs_node *dir = NULL;
    struct resource *res = vfs_open(&dir, parent, path, creat_flags, mode);

    if (res == NULL) {
        ctx->rax = (uint64_t)-1;
        return;
    }

    int ret = fd_create_from_resource(dir, res, flags, -1);

    ctx->rax = (uint64_t)ret;
}

void syscall_close(struct cpu_gpr_context *ctx) {
    int fildes = (int)ctx->rdi;

    int ret = fd_close(fildes);

    ctx->rax = (uint64_t)ret;
}

void syscall_read(struct cpu_gpr_context *ctx) {
    int    fd    = (int)    ctx->rdi;
    void  *buf   = (void *) ctx->rsi;
    size_t count = (size_t) ctx->rdx;

    struct handle *handle = handle_from_fd(fd);

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

    struct handle *handle = handle_from_fd(fd);

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

    struct resource *res = resource_from_fd(fd);

    int ret = res->ioctl(res, request, argp);

    if (ret == -1) {
        ctx->rax = (uint64_t)-1;
        return;
    }

    ctx->rax = (uint64_t)ret;
}

void syscall_chdir(struct cpu_gpr_context *ctx) {
    const char *path = (const char *)ctx->rdi;

    struct process *process = this_cpu->current_thread->process;

    SPINLOCK_ACQUIRE(vfs_lock);

    struct vfs_node *new_dir = path2node(process->current_directory, path, NO_CREATE);

    if (new_dir == NULL) {
        ctx->rax = (uint64_t)-1;
        LOCK_RELEASE(vfs_lock);
        return;
    }

    if (!S_ISDIR(new_dir->res->st.st_mode)) {
        ctx->rax = (uint64_t)-1;
        errno = ENOTDIR;
        LOCK_RELEASE(vfs_lock);
        return;
    }

    process->current_directory = new_dir;
    ctx->rax = 0;
    LOCK_RELEASE(vfs_lock);
}

void syscall_mkdirat(struct cpu_gpr_context *ctx) {
    int         dirfd = (int)          ctx->rdi;
    const char *path  = (const char *) ctx->rsi;
    mode_t      mode  = (mode_t)       ctx->rdx;

    struct vfs_node *parent = get_parent_dir(dirfd, path);
    if (parent == NULL) {
        ctx->rax = (uint64_t)-1;
        return;
    }

    struct vfs_node *new_dir = vfs_mkdir(parent, path, mode, false);

    if (new_dir == NULL) {
        ctx->rax = (uint64_t)-1;
        return;
    }

    ctx->rax = 0;
}

void syscall_faccessat(struct cpu_gpr_context *ctx) {
    int         dirfd = (int)          ctx->rdi;
    const char *path  = (const char *) ctx->rsi;
    int         mode  = (int)          ctx->rdx;
    int         flags = (int)          ctx->r10;

    SPINLOCK_ACQUIRE(vfs_lock);

    struct vfs_node *parent = get_parent_dir(dirfd, path);
    if (parent == NULL) {
        ctx->rax = (uint64_t)-1;
        LOCK_RELEASE(vfs_lock);
        return;
    }

    struct vfs_node *node = path2node(parent, path, NO_CREATE);
    if (node == NULL) {
        ctx->rax = (uint64_t)-1;
        LOCK_RELEASE(vfs_lock);
        return;
    }

    LOCK_RELEASE(vfs_lock);
    ctx->rax = 0;
}

#define SEEK_CUR 1
#define SEEK_END 2
#define SEEK_SET 3

void syscall_seek(struct cpu_gpr_context *ctx) {
    int   fd     = (int)   ctx->rdi;
    off_t offset = (off_t) ctx->rsi;
    int   whence = (int)   ctx->rdx;

    SPINLOCK_ACQUIRE(vfs_lock);

    struct handle *handle = handle_from_fd(fd);

    if (handle == NULL) {
        LOCK_RELEASE(vfs_lock);
        ctx->rax = (uint64_t)-1;
        return;
    }

    switch (handle->res->st.st_mode & S_IFMT) {
        case S_IFCHR: case S_IFIFO: case S_IFPIPE: case S_IFSOCK:
            errno = ESPIPE;
            ctx->rax = (uint64_t)-1;
            LOCK_RELEASE(vfs_lock);
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
            LOCK_RELEASE(vfs_lock);
            return;
    }

    if (base < 0 || base > handle->res->st.st_size) {
        errno = EINVAL;
        ctx->rax = (uint64_t)-1;
        LOCK_RELEASE(vfs_lock);
        return;
    }

    handle->loc = base;
    ctx->rax = (uint64_t)base;
    LOCK_RELEASE(vfs_lock);
}

struct resource *vfs_open(struct vfs_node **dir, struct vfs_node *parent, const char *path, int oflags, mode_t mode) {
    SPINLOCK_ACQUIRE(vfs_lock);

    parent = parent == NULL ? this_cpu->current_thread->process->current_directory
           : parent;

    bool create = oflags & O_CREAT;
    struct vfs_node *path_node = path2node(parent, path, create ? CREATE_SHALLOW : NO_CREATE);
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

    if (S_ISDIR(res->st.st_mode) && dir != NULL) {
        *dir = path_node;
    }

    SPINLOCK_ACQUIRE(res->lock);
    res->refcount++;
    LOCK_RELEASE(res->lock);

    LOCK_RELEASE(vfs_lock);

    return res;
}

bool vfs_symlink(struct vfs_node *parent, const char *target, const char *path) {
    SPINLOCK_ACQUIRE(vfs_lock);

    struct vfs_node *path_node = path2node(parent, path,
                                           FAIL_IF_EXISTS | CREATE_SHALLOW);

    if (path_node == NULL) {
        LOCK_RELEASE(vfs_lock);
        return false;
    }

    strcpy(path_node->target, target);

    path_node->res = path_node->fs->symlink(path_node);

    LOCK_RELEASE(vfs_lock);

    return true;
}

void vfs_dump_nodes(struct vfs_node *node, const char *parent) {
    struct vfs_node *cur_node = node ? node : vfs_root_node;
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

void syscall_fstat(struct cpu_gpr_context *ctx) {
    int          fd      = (int)           ctx->rdi;
    struct stat *statbuf = (struct stat *) ctx->rsi;

    struct resource *res = resource_from_fd(fd);

    *statbuf = res->st;

    ctx->rax = 0;
}

void syscall_fstatat(struct cpu_gpr_context *ctx) {
    int          dirfd   = (int)           ctx->rdi;
    const char  *path    = (const char *)  ctx->rsi;
    struct stat *statbuf = (struct stat *) ctx->rdx;
    int          flags   = (int)           ctx->r10;

    struct vfs_node *parent = get_parent_dir(dirfd, path);
    if (parent == NULL) {
        ctx->rax = (uint64_t)-1;
        return;
    }

    bool ret = vfs_stat(parent, path, statbuf, flags);
    if (ret == false) {
        ctx->rax = (uint64_t)-1;
        return;
    }

    ctx->rax = 0;
}

bool vfs_stat(struct vfs_node *parent, const char *path, struct stat *st, int flags) {
    SPINLOCK_ACQUIRE(vfs_lock);

    struct vfs_node *node = path2node(parent, path, NO_CREATE);
    if (node == NULL) {
        LOCK_RELEASE(vfs_lock);
        return false;
    }

    *st = node->res->st;

    LOCK_RELEASE(vfs_lock);
    return true;
}
