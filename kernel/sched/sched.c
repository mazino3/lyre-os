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
#include <fs/vfs.h>

#define THREAD_STACK_SIZE ((size_t)8192)
#define THREAD_STACK_TOP  ((uintptr_t)0x70000000000)

static lock_t sched_lock = {0};

static uint8_t reschedule_vector;

DYNARRAY_STATIC(struct process *, processes);

DYNARRAY_STATIC(struct thread *, threads);
DYNARRAY_STATIC(struct thread *, running_queue);
DYNARRAY_STATIC(struct thread *, idle_queue);

struct process *sched_start_program(const char *path,
                                    const char **argv,
                                    const char **envp,
                                    const char *stdin,
                                    const char *stdout,
                                    const char *stderr) {
    struct resource *file = vfs_open(path, O_RDONLY, 0);
    if (file == NULL)
        return NULL;

    struct pagemap *new_pagemap = vmm_new_pagemap(PAGING_4LV);

    struct process *new_process = sched_new_process(new_pagemap);

    struct auxval_t auxval;
    char *ld_path;

    if (!elf_load(new_pagemap, file, 0, &auxval, &ld_path))
        return NULL;

    file->close(file);

    void *entry_point;

    if (ld_path == NULL) {
        entry_point = (void *)auxval.at_entry;
    } else {
        struct resource *ld = vfs_open(ld_path, O_RDONLY, 0);

        struct auxval_t ld_auxval;
        if (!elf_load(new_pagemap, ld, 0x40000000, &ld_auxval, NULL))
            return NULL;

        ld->close(ld);

        free(ld_path);

        entry_point = (void *)ld_auxval.at_entry;
    }

    // open stdin, stdout, and stderr
    struct resource *stdin_res  = vfs_open(stdin,  O_RDONLY, 0);
    struct resource *stdout_res = vfs_open(stdout, O_WRONLY, 0);
    struct resource *stderr_res = vfs_open(stderr, O_WRONLY, 0);

    struct handle *stdin_handle  = alloc(sizeof(struct handle));
    stdin_handle->res = stdin_res;
    struct handle *stdout_handle = alloc(sizeof(struct handle));
    stdout_handle->res = stdout_res;
    struct handle *stderr_handle = alloc(sizeof(struct handle));
    stderr_handle->res = stderr_res;

    DYNARRAY_INSERT(new_process->handles, stdin_handle);
    DYNARRAY_INSERT(new_process->handles, stdout_handle);
    DYNARRAY_INSERT(new_process->handles, stderr_handle);

    sched_new_thread(new_process, true, entry_point, NULL,
                     argv, envp, &auxval);

    return new_process;
}

struct process *sched_new_process(struct pagemap *pagemap) {
    struct process *new_process = alloc(sizeof(struct process));
    if (new_process == NULL)
        return NULL;

    new_process->pagemap = pagemap;
    new_process->thread_stack_top = THREAD_STACK_TOP;
    new_process->mmap_anon_non_fixed_base = MMAP_ANON_NON_FIXED_BASE;

    SPINLOCK_ACQUIRE(sched_lock);

    pid_t pid = DYNARRAY_INSERT(processes, new_process);
    new_process->pid = pid;

    LOCK_RELEASE(sched_lock);

    return new_process;
}

struct thread *sched_new_thread(struct process *proc,
                                bool want_elf,
                                void *addr,
                                void *arg,
                                const char **argv,
                                const char **envp,
                                struct auxval_t *auxval) {
    struct thread *new_thread = alloc(sizeof(struct thread));
    if (new_thread == NULL)
        return NULL;

    new_thread->ctx.rflags = 0x202;

    size_t *stack = pmm_allocz(THREAD_STACK_SIZE / PAGE_SIZE);

    uintptr_t stack_bottom_vma;
    if (proc != NULL) {
        // User thread
        new_thread->ctx.cs = 0x23;
        new_thread->ctx.ss = 0x1b;

        proc->thread_stack_top -= THREAD_STACK_SIZE;
        stack_bottom_vma = proc->thread_stack_top;
        proc->thread_stack_top -= PAGE_SIZE;

        for (size_t i = 0; i < THREAD_STACK_SIZE; i += PAGE_SIZE) {
            vmm_map_page(proc->pagemap, stack_bottom_vma + i,
                         (uintptr_t)stack + i, 0x07);
        }

        void *kernel_stack = pmm_allocz(THREAD_STACK_SIZE / PAGE_SIZE);
        new_thread->kernel_stack =
                (uintptr_t)kernel_stack + THREAD_STACK_SIZE + MEM_PHYS_OFFSET;
    } else {
        // Kernel thread
        new_thread->ctx.cs = 0x08;
        new_thread->ctx.ss = 0x10;

        stack_bottom_vma = (uintptr_t)stack + MEM_PHYS_OFFSET;
    }

    new_thread->ctx.rsp = stack_bottom_vma + THREAD_STACK_SIZE;

    stack = (void *)stack + THREAD_STACK_SIZE + MEM_PHYS_OFFSET;

    if (want_elf) {
        uintptr_t stack_top = (uintptr_t)stack;

        /* Push all strings onto the stack. */
        size_t nenv = 0;
        for (const char **elem = envp; *elem; elem++) {
            stack = (void*)stack - (strlen(*elem) + 1);
            strcpy(stack, *elem);
            nenv++;
        }
        size_t nargs = 0;
        for (const char **elem = argv; *elem; elem++) {
            stack = (void*)stack - (strlen(*elem) + 1);
            strcpy(stack, *elem);
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

    new_thread->process = proc;

    new_thread->timeslice = 5000;

    SPINLOCK_ACQUIRE(sched_lock);

    tid_t thread_id = DYNARRAY_INSERT(threads, new_thread);
    new_thread->tid = thread_id;

    DYNARRAY_PUSHBACK(running_queue, new_thread);

    if (proc != NULL) {
        DYNARRAY_INSERT(proc->threads, new_thread);
    }

    LOCK_RELEASE(sched_lock);

    return new_thread;
}

static ssize_t get_next_thread(ssize_t index) {
    if (index == -1) {
        index = 0;
    } else {
        index++;
    }

    for (size_t i = 0; i < running_queue.length + 1; i++) {
        if ((size_t)index >= running_queue.length) {
            index = 0;
        }
        struct thread *thread = running_queue.storage[index];
        if (LOCK_ACQUIRE(thread->lock)) {
            return index;
        }
        index++;
    }

    return -1;
}

__attribute__((noreturn))
void sched_spinup(struct cpu_gpr_context *);

__attribute__((noreturn))
void reschedule(struct cpu_gpr_context *ctx) {
    SPINLOCK_ACQUIRE(sched_lock);

    if (ctx->cs & 0x03) {
        swapgs();
    }

    struct cpu_local *cpu_local      = this_cpu;
    struct thread    *current_thread = cpu_local->current_thread;

    if (current_thread != NULL) {
        current_thread->ctx = *ctx;
        current_thread->user_gs = get_user_gs();
        current_thread->user_fs = get_user_fs();
        LOCK_RELEASE(current_thread->lock);
    }

    ssize_t next_thread_index = get_next_thread(cpu_local->last_run_queue_index);

    LOCK_RELEASE(sched_lock);

    cpu_local->last_run_queue_index = next_thread_index;

    if (next_thread_index == -1) {
        // We're idle, get a reschedule interrupt in 20 milliseconds
        lapic_eoi();
        lapic_timer_oneshot(reschedule_vector, 20000);
        cpu_local->current_thread = NULL;
        asm ("sti");
        for (;;) asm ("hlt");
    }

    current_thread = running_queue.storage[next_thread_index];
    cpu_local->current_thread = current_thread;

    set_user_gs(current_thread->user_gs);
    set_user_fs(current_thread->user_fs);

    cpu_local->kernel_stack = current_thread->kernel_stack;

    lapic_eoi();
    lapic_timer_oneshot(reschedule_vector, current_thread->timeslice);

    if (current_thread->ctx.cs & 0x03) {
        swapgs();
    }

    if (current_thread->process != NULL)
        vmm_switch_pagemap(current_thread->process->pagemap);
    else
        vmm_switch_pagemap(kernel_pagemap);

    sched_spinup(&current_thread->ctx);
}

void _reschedule(void);

void sched_init(void) {
    static bool got_vector = false;
    if (!got_vector) {
        reschedule_vector = idt_get_empty_int_vector();
        idt_register_interrupt_handler(reschedule_vector, _reschedule, 1, 0x8e);
        got_vector = true;
        print("sched: Scheduler interrupt vector is %x\n", reschedule_vector);
    }
    lapic_timer_oneshot(reschedule_vector, 20000);
}
