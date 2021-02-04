#include <stdint.h>
#include <stddef.h>
#include <sched/sched.h>
#include <lib/dynarray.h>
#include <lib/lock.h>
#include <lib/elf.h>
#include <mm/pmm.h>
#include <mm/vmm.h>
#include <sys/idt.h>
#include <sys/apic.h>
#include <lib/print.h>
#include <lib/errno.h>
#include <fs/vfs.h>

#define THREAD_STACK_SIZE ((size_t)32768)
#define THREAD_STACK_TOP  ((uintptr_t)0x70000000000)

static lock_t sched_lock = {0};

static uint8_t reschedule_vector;

DYNARRAY_STATIC(struct process *, processes);

#define MAX_RUNNING_THREADS 8192
static struct thread *running_queue[MAX_RUNNING_THREADS];

struct process *kernel_process;

void syscall_getpid(struct cpu_gpr_context *ctx) {
    ctx->rax = (uint64_t)this_cpu->current_thread->process->pid;
}

void syscall_getppid(struct cpu_gpr_context *ctx) {
    ctx->rax = (uint64_t)this_cpu->current_thread->process->ppid;
}

static void init_process(struct process *process) {
    process->thread_stack_top = THREAD_STACK_TOP;
    process->mmap_anon_non_fixed_base = MMAP_ANON_NON_FIXED_BASE;
}

struct process *sched_start_program(bool execve,
                                    const char *path,
                                    const char **argv,
                                    const char **envp,
                                    const char *stdin,
                                    const char *stdout,
                                    const char *stderr) {
    struct resource *file = vfs_open(NULL, NULL, path, O_RDONLY, 0);
    if (file == NULL)
        return NULL;

    struct pagemap *new_pagemap = vmm_new_pagemap();

    struct auxval_t auxval;
    char *ld_path;

    if (!elf_load(new_pagemap, file, 0, &auxval, &ld_path))
        return NULL;

    file->close(file);

    void *entry_point;

    if (ld_path == NULL) {
        entry_point = (void *)auxval.at_entry;
    } else {
        struct resource *ld = vfs_open(NULL, NULL, ld_path, O_RDONLY, 0);

        struct auxval_t ld_auxval;
        if (!elf_load(new_pagemap, ld, 0x40000000, &ld_auxval, NULL))
            return NULL;

        ld->close(ld);

        free(ld_path);

        entry_point = (void *)ld_auxval.at_entry;
    }

    struct process *new_process;
    if (!execve) {
        new_process = sched_new_process(NULL, new_pagemap);
        new_process->ppid = this_cpu->current_thread->process->pid;

        struct resource *stdin_res  = vfs_open(NULL, NULL, stdin,  O_RDONLY, 0);
        struct handle *stdin_handle  = alloc(sizeof(struct handle));
        stdin_handle->res = stdin_res;
        struct file_descriptor *stdin_fd = alloc(sizeof(struct file_descriptor));
        stdin_fd->handle = stdin_handle;
        DYNARRAY_INSERT(new_process->fds, stdin_fd);

        struct resource *stdout_res = vfs_open(NULL, NULL, stdout, O_WRONLY, 0);
        struct handle *stdout_handle = alloc(sizeof(struct handle));
        stdout_handle->res = stdout_res;
        struct file_descriptor *stdout_fd = alloc(sizeof(struct file_descriptor));
        stdout_fd->handle = stdout_handle;
        DYNARRAY_INSERT(new_process->fds, stdout_fd);

        struct resource *stderr_res = vfs_open(NULL, NULL, stderr, O_WRONLY, 0);
        struct handle *stderr_handle = alloc(sizeof(struct handle));
        stderr_handle->res = stderr_res;
        struct file_descriptor *stderr_fd = alloc(sizeof(struct file_descriptor));
        stderr_fd->handle = stderr_handle;
        DYNARRAY_INSERT(new_process->fds, stderr_fd);

        sched_new_thread(NULL, new_process, true, entry_point, NULL,
                         argv, envp, &auxval, true, NULL);
    } else {
        struct thread  *thread  = this_cpu->current_thread;
        struct process *process = thread->process;
        struct pagemap *old_pagemap = process->pagemap;
        init_process(process);

        sched_new_thread(thread, process, true, entry_point, NULL,
                         argv, envp, &auxval, false, new_pagemap);

        process->pagemap = new_pagemap;
        vmm_switch_pagemap(new_pagemap);
        vmm_erase_pagemap(old_pagemap);

        asm ("cli");
        set_user_gs(0);
        set_user_fs(0);
        swapgs();
        sched_spinup(&thread->ctx);
    }

    return new_process;
}

void syscall_execve(struct cpu_gpr_context *ctx) {
    const char *path = (const char *) ctx->rdi;
    const char      **argv = (const char **)      ctx->rsi;
    const char      **envp = (const char **)      ctx->rdx;

    sched_start_program(true, path, argv, envp, NULL, NULL, NULL);

    // If we got here, we effed up
    ctx->rax = (uint64_t)-1;
}

void syscall_fork(struct cpu_gpr_context *ctx) {
    struct process *old_process = this_cpu->current_thread->process;

    struct process *new_process = sched_new_process(old_process, NULL);

    struct thread *new_thread = alloc(sizeof(struct thread));

    new_thread->ctx = *ctx;
    new_thread->ctx.rax = (uint64_t)0;

    new_thread->process = new_process;

    new_thread->timeslice = this_cpu->current_thread->timeslice;

    new_thread->user_gs = this_cpu->current_thread->user_gs;
    new_thread->user_fs = this_cpu->current_thread->user_fs;

    void *kernel_stack = pmm_allocz(THREAD_STACK_SIZE / PAGE_SIZE);
    new_thread->kernel_stack = (uintptr_t)kernel_stack + THREAD_STACK_SIZE + MEM_PHYS_OFFSET;

    SPINLOCK_ACQUIRE(sched_lock);

    sched_queue(new_thread);

    new_thread->tid = DYNARRAY_INSERT(new_process->threads, new_thread);

    DYNARRAY_INSERT(old_process->children, new_process);

    LOCK_RELEASE(sched_lock);

    ctx->rax = (uint64_t)new_process->pid;
}

// defines from mlibc...

#define WCONTINUED 1
#define WNOHANG 2
#define WUNTRACED 4
#define WEXITED 8
#define WNOWAIT 16
#define WSTOPPED 32

#define WCOREFLAG 0x80

#define WEXITSTATUS(x) ((x) & 0x000000FF)
#define WIFCONTINUED(x) ((x) & 0x00000100)
#define WIFEXITED(x) ((x) & 0x00000200)
#define WIFSIGNALED(x) ((x) & 0x00000400)
#define WIFSTOPPED(x) ((x) & 0x00000800)
#define WSTOPSIG(x) (((x) & 0x00FF0000) >> 16)
#define WTERMSIG(x) (((x) & 0xFF000000) >> 24)
#define WCOREDUMP(x) ((x) & WCOREFLAG)

void syscall_waitpid(struct cpu_gpr_context *ctx) {
    pid_t pid     = (pid_t) ctx->rdi;
    int  *status  = (int *) ctx->rsi;
    int   options = (int)   ctx->rdx;

    struct process *process = this_cpu->current_thread->process;

    struct event **events;

    if (pid == -1) {
        events = alloc(process->children.length * sizeof(struct event *));
        for (size_t i = 0; i < process->children.length; i++) {
            events[i] = process->children.storage[i]->event;
        }
    } else {
        print("\nwaitpid: value of pid %d not supported\n", pid);
        errno = EINVAL;
        ctx->rax = (uint64_t)-1;
        return;
    }

    ssize_t which;
    events_await(events, &which, process->children.length,
                 options & WNOHANG);
    free(events);

    if (which == -1) {
        ctx->rax = (uint64_t)0;
        return;
    }

    struct process *child = process->children.storage[which];

    *status = child->status;
    ctx->rax = (uint64_t)child->pid;

    processes.storage[child->pid] = NULL;

    DYNARRAY_REMOVE_AND_PACK(process->children, which);

    free(child);
}

__attribute__((noreturn))
void syscall_exit(struct cpu_gpr_context *ctx) {
    int status = (int) ctx->rdi;

    struct process *process = this_cpu->current_thread->process;

    struct pagemap *old_pagemap = process->pagemap;

    process->pagemap = kernel_pagemap;
    vmm_switch_pagemap(kernel_pagemap);

    // Close all fds
    for (size_t i = 0; i < process->fds.length; i++) {
        fd_close(i);
    }

    // PID 1 inherits children
    for (size_t i = 0; i < process->children.length; i++) {
        DYNARRAY_INSERT(processes.storage[1]->children,
                        process->children.storage[i]);
    }

    vmm_erase_pagemap(old_pagemap);

    // Set WIFEXITED
    status |= 0x00000200;

    LOCKED_WRITE(process->status, status);
    event_trigger(process->event);

    sched_dequeue_and_die();

    for (;;);
}

struct process *sched_new_process(struct process *old_process, struct pagemap *pagemap) {
    struct process *new_process = alloc(sizeof(struct process));
    if (new_process == NULL)
        return NULL;

    if (old_process == NULL) {
        new_process->ppid = 0;

        new_process->pagemap = pagemap;
        new_process->current_directory = vfs_root_node;

        init_process(new_process);
    } else {
        new_process->ppid = old_process->pid;

        new_process->pagemap = vmm_fork_pagemap(old_process->pagemap);
        new_process->thread_stack_top = old_process->thread_stack_top;
        new_process->mmap_anon_non_fixed_base = old_process->mmap_anon_non_fixed_base;
        new_process->current_directory = old_process->current_directory;

        for (size_t i = 0; i < old_process->fds.length; i++) {
            if (old_process->fds.storage[i] == NULL)
                continue;
            struct file_descriptor *fd = alloc(sizeof(struct file_descriptor));
            *fd = *old_process->fds.storage[i];
            fd->handle->refcount++;
            DYNARRAY_PUSHBACK(new_process->fds, fd);
        }
    }

    new_process->event = event_create(1);

    SPINLOCK_ACQUIRE(sched_lock);

    pid_t pid = DYNARRAY_INSERT(processes, new_process);

    new_process->pid = pid;

    LOCK_RELEASE(sched_lock);

    return new_process;
}

struct thread *sched_new_thread(struct thread *new_thread,
                                struct process *proc,
                                bool want_elf,
                                void *addr,
                                void *arg,
                                const char **argv,
                                const char **envp,
                                struct auxval_t *auxval,
                                bool start,
                                struct pagemap *new_pagemap) {
    if (new_thread == NULL) {
        new_thread = alloc(sizeof(struct thread));
        if (new_thread == NULL)
            return NULL;
    }

    if (new_pagemap == NULL)
        new_pagemap = proc->pagemap;

    memset(&new_thread->ctx, 0, sizeof(struct cpu_gpr_context));
    new_thread->user_gs = 0;
    new_thread->user_fs = 0;

    new_thread->ctx.rflags = 0x202;

    size_t *stack = pmm_allocz(THREAD_STACK_SIZE / PAGE_SIZE);

    if (new_thread->page_fault_stack == 0) {
        new_thread->page_fault_stack = (uintptr_t)pmm_allocz(THREAD_STACK_SIZE / PAGE_SIZE);
        new_thread->page_fault_stack += THREAD_STACK_SIZE + MEM_PHYS_OFFSET;
    }

    uintptr_t stack_bottom_vma;
    if (proc != kernel_process) {
        // User thread
        new_thread->ctx.cs = 0x23;
        new_thread->ctx.ss = 0x1b;

        proc->thread_stack_top -= THREAD_STACK_SIZE;
        stack_bottom_vma = proc->thread_stack_top;
        proc->thread_stack_top -= PAGE_SIZE;

        for (size_t i = 0; i < THREAD_STACK_SIZE; i += PAGE_SIZE) {
            vmm_map_page(new_pagemap, stack_bottom_vma + i,
                         (uintptr_t)stack + i, 0x07);
        }

        if (new_thread->kernel_stack == 0) {
            void *kernel_stack = pmm_allocz(THREAD_STACK_SIZE / PAGE_SIZE);
            new_thread->kernel_stack =
                    (uintptr_t)kernel_stack + THREAD_STACK_SIZE + MEM_PHYS_OFFSET;
        }
    } else {
        // Kernel thread
        new_thread->ctx.cs = 0x08;
        new_thread->ctx.ss = 0x10;

        stack_bottom_vma = (uintptr_t)stack + MEM_PHYS_OFFSET;
    }

    new_thread->process = proc;

    new_thread->ctx.rsp = stack_bottom_vma + THREAD_STACK_SIZE;

    stack = (void *)stack + THREAD_STACK_SIZE + MEM_PHYS_OFFSET;

    if (want_elf) {
        uintptr_t stack_top = (uintptr_t)stack;

        /* Push all strings onto the stack. */
        size_t nenv = 0;
        for (const char **elem = envp; *elem; elem++) {
            stack = (void*)stack - (strlen(*elem) + 1);
            strcpy((char*)stack, *elem);
            nenv++;
        }
        size_t nargs = 0;
        for (const char **elem = argv; *elem; elem++) {
            stack = (void*)stack - (strlen(*elem) + 1);
            strcpy((char*)stack, *elem);
            nargs++;
        }

        /* Align strp to 16-byte so that the following calculation becomes easier. */
        stack = (void*)stack - ((uintptr_t)stack & 0xf);

        /* Make sure the *final* stack pointer is 16-byte aligned.
            - The auxv takes a multiple of 16-bytes; ignore that.
            - There are 2 markers that each take 8-byte; ignore that, too.
            - Then, there is argc and (nargs + nenv)-many pointers to args/environ.
              Those are what we *really* care about. */
        if ((nargs + nenv + 1) & 1)
            stack--;

        *(--stack) = 0; *(--stack) = 0; /* Zero auxiliary vector entry */
        stack -= 2; *stack = AT_ENTRY;  *(stack + 1) = auxval->at_entry;
        stack -= 2; *stack = AT_PHDR;   *(stack + 1) = auxval->at_phdr;
        stack -= 2; *stack = AT_PHENT;  *(stack + 1) = auxval->at_phent;
        stack -= 2; *stack = AT_PHNUM;  *(stack + 1) = auxval->at_phnum;

        uintptr_t sa = new_thread->ctx.rsp;

        *(--stack) = 0; /* Marker for end of environ */
        stack -= nenv;
        for (size_t i = 0; i < nenv; i++) {
            sa -= strlen(envp[i]) + 1;
            stack[i] = sa;
        }

        *(--stack) = 0; /* Marker for end of argv */
        stack -= nargs;
        for (size_t i = 0; i < nargs; i++) {
            sa -= strlen(argv[i]) + 1;
            stack[i] = sa;
        }
        *(--stack) = nargs; /* argc */

        new_thread->ctx.rsp -= stack_top - (uintptr_t)stack;
    } else {
        new_thread->ctx.rdi = (uint64_t)arg;
    }

    new_thread->ctx.rip = (uintptr_t)addr;

    new_thread->timeslice = 5000;

    if (start) {
        SPINLOCK_ACQUIRE(sched_lock);
        sched_queue(new_thread);
        new_thread->tid = DYNARRAY_INSERT(proc->threads, new_thread);
        LOCK_RELEASE(sched_lock);
    }

    return new_thread;
}

void sched_yield(void) {
    asm ("cli");
    LOCKED_WRITE(this_cpu->yield_await, 1);
    lapic_timer_oneshot(reschedule_vector, 1);
    asm ("sti");
    while (LOCKED_READ(this_cpu->yield_await) != 0)
        asm ("hlt");
}

bool sched_queue(struct thread *thread) {
    if (thread->is_queued == true)
        return true;
    for (size_t i = 0; i < MAX_RUNNING_THREADS; i++) {
        if (CAS(running_queue[i], NULL, thread)) {
            thread->is_queued = true;
            return true;
        }
    }
    return false;
}

bool sched_dequeue(struct thread *thread) {
    if (thread->is_queued == false)
        return true;
    for (size_t i = 0; i < MAX_RUNNING_THREADS; i++) {
        if (CAS(running_queue[i], thread, NULL)) {
            thread->is_queued = false;
            return true;
        }
    }
    return false;
}

void sched_dequeue_and_yield(void) {
    asm ("cli");
    sched_dequeue(this_cpu->current_thread);
    sched_yield();
}

__attribute__((noreturn))
void sched_dequeue_and_die(void) {
    asm ("cli");
    sched_dequeue(this_cpu->current_thread);
    free(this_cpu->current_thread);
    this_cpu->current_thread = NULL;
    sched_yield();
    for (;;);
}

static struct thread *get_next_thread(ssize_t *index) {
    if (*index == -1) {
        *index = 0;
    }

    ssize_t i = *index + 1;
    for (;;) {
        if (i == MAX_RUNNING_THREADS) {
            i = 0;
        }
        struct thread *thread = LOCKED_READ(running_queue[i]);
        if (thread != NULL && LOCK_ACQUIRE(thread->lock)) {
            *index = i;
            return thread;
        }
        i++;
        if (i == *index + 1)
            break;
    }

    *index = -1;
    return NULL;
}

void reschedule(struct cpu_gpr_context *ctx) {
    if (ctx->cs & 0x03) {
        swapgs();
    }

    struct cpu_local *cpu_local      = this_cpu;
    struct thread    *current_thread = cpu_local->current_thread;

    if (!LOCK_ACQUIRE(sched_lock)) {
        int slice = current_thread != NULL ? current_thread->timeslice : 20000;
        lapic_eoi();
        lapic_timer_oneshot(reschedule_vector, slice);
        if (ctx->cs & 0x03) {
            swapgs();
        }
        return;
    }

    LOCKED_WRITE(this_cpu->yield_await, 0);

    if (current_thread != NULL) {
        current_thread->ctx = *ctx;
        current_thread->user_gs = get_user_gs();
        current_thread->user_fs = get_user_fs();
        current_thread->user_stack = cpu_local->user_stack;
        LOCK_RELEASE(current_thread->lock);
    }

    cpu_local->current_thread =
            get_next_thread(&cpu_local->last_run_queue_index);
    current_thread = cpu_local->current_thread;

    if (current_thread == NULL) {
        // We're idle, get a reschedule interrupt in 20 milliseconds
        lapic_eoi();
        lapic_timer_oneshot(reschedule_vector, 20000);
        LOCK_RELEASE(sched_lock);
        asm ("sti");
        for (;;) asm ("hlt");
    }

    set_user_gs(current_thread->user_gs);
    set_user_fs(current_thread->user_fs);

    cpu_local->user_stack   = current_thread->user_stack;
    cpu_local->kernel_stack = current_thread->kernel_stack;

    cpu_local->tss.ist2 = current_thread->page_fault_stack;

    LOCK_RELEASE(sched_lock);

    lapic_eoi();
    lapic_timer_oneshot(reschedule_vector, current_thread->timeslice);

    if (current_thread->ctx.cs & 0x03) {
        swapgs();
    }

    vmm_switch_pagemap(current_thread->process->pagemap);

    sched_spinup(&current_thread->ctx);
}

void _reschedule(void);

__attribute__((noreturn))
void sched_wait(void) {
    lapic_timer_oneshot(reschedule_vector, 20000);
    asm ("sti");
    for (;;) asm ("hlt");
}

void sched_init(void) {
    reschedule_vector = idt_get_empty_int_vector();
    idt_register_interrupt_handler(reschedule_vector, _reschedule, 1, 0x8e);
    print("sched: Scheduler interrupt vector is %x\n", reschedule_vector);

    kernel_process = alloc(sizeof(struct process));
    kernel_process->pagemap = kernel_pagemap;
    kernel_process->current_directory = vfs_root_node;
    DYNARRAY_INSERT(processes, kernel_process);
}
