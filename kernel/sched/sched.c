#include <stdint.h>
#include <stddef.h>
#include <sched/sched.h>
#include <lib/dynarray.h>
#include <lib/lock.h>
#include <lib/elf.h>
#include <mm/pmm.h>
#include <mm/vmm.h>

#define THREAD_STACK_SIZE ((size_t)8192)
#define THREAD_STACK_TOP  ((uintptr_t)0x70000000000)

static lock_t sched_lock = {0};

DYNARRAY_STATIC(struct process *, processes);

DYNARRAY_STATIC(struct thread *, threads);
DYNARRAY_STATIC(struct thread *, running_queue);
DYNARRAY_STATIC(struct thread *, idle_queue);

static struct cpu_gpr_context default_ctx = {
    .rax = 0, .rbx = 0, .rcx = 0, .rdx = 0, .rsi = 0, .rdi = 0, .rbp = 0,
    .r8  = 0, .r9  = 0, .r10 = 0, .r11 = 0, .r12 = 0, .r13 = 0, .r14 = 0, .r15 = 0,
    .rip = 0, .cs = 0x08, .rflags = 0x202, .rsp = 0, .ss = 0x10
};

struct process *sched_new_process() {
    struct process *new_process = alloc(sizeof(struct process));
    if (new_process == NULL)
        return NULL;

    new_process->thread_stack_top = THREAD_STACK_TOP;
}

static uintptr_t kernel_stack_top = 0xfffffffffffff000;

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

    new_thread->ctx = default_ctx;

    size_t *stack = pmm_alloc(THREAD_STACK_SIZE / PAGE_SIZE);

    uintptr_t stack_vma;
    if (proc != NULL) {
        proc->thread_stack_top -= THREAD_STACK_SIZE;
        stack_vma = proc->thread_stack_top;
        proc->thread_stack_top -= PAGE_SIZE;
    } else {
        kernel_stack_top -= THREAD_STACK_SIZE;
        stack_vma = kernel_stack_top;
        kernel_stack_top -= PAGE_SIZE;
    }

    for (size_t i = 0; i < THREAD_STACK_SIZE; i += PAGE_SIZE) {
        vmm_map_page(proc == NULL ? kernel_pagemap : proc->pagemap,
                     stack_vma + i,
                     (uintptr_t)stack + i,
                     proc == NULL ? 0x03 : 0x07);
    }

    new_thread->ctx.rsp = stack_vma + THREAD_STACK_SIZE;

    stack = (uintptr_t)stack + THREAD_STACK_SIZE + MEM_PHYS_OFFSET;

    if (want_elf) {
        // TODO
    } else {
        // TODO
    }

    new_thread->ctx.rip = (uintptr_t)addr;

    new_thread->process = proc;

    SPINLOCK_ACQUIRE(sched_lock);

    DYNARRAY_PUSHBACK(running_queue, new_thread);

    LOCK_RELEASE(sched_lock);

    return new_thread;
}

void schedule(struct cpu_gpr_context *ctx) {




}






