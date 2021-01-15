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

#define THREAD_STACK_SIZE ((size_t)8192)
#define THREAD_STACK_TOP  ((uintptr_t)0x70000000000)

static lock_t sched_lock = {0};

static uint8_t reschedule_vector;

DYNARRAY_STATIC(struct process *, processes);

DYNARRAY_STATIC(struct thread *, threads);
DYNARRAY_STATIC(struct thread *, running_queue);
DYNARRAY_STATIC(struct thread *, idle_queue);

struct process *sched_new_process() {
    struct process *new_process = alloc(sizeof(struct process));
    if (new_process == NULL)
        return NULL;

    new_process->thread_stack_top = THREAD_STACK_TOP;

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

    uintptr_t stack_vma;
    if (proc != NULL) {
        // User thread
        new_thread->ctx.cs = 0x20;
        new_thread->ctx.ss = 0x18;

        proc->thread_stack_top -= THREAD_STACK_SIZE;
        stack_vma = proc->thread_stack_top;
        proc->thread_stack_top -= PAGE_SIZE;

        for (size_t i = 0; i < THREAD_STACK_SIZE; i += PAGE_SIZE) {
            vmm_map_page(proc == NULL ? kernel_pagemap : proc->pagemap,
                         stack_vma + i,
                         (uintptr_t)stack + i,
                         proc == NULL ? 0x03 : 0x07);
        }

        void *kernel_stack = pmm_allocz(THREAD_STACK_SIZE / PAGE_SIZE);
        new_thread->kernel_stack =
                (uintptr_t)kernel_stack + THREAD_STACK_SIZE + MEM_PHYS_OFFSET;
    } else {
        // Kernel thread
        new_thread->ctx.cs = 0x08;
        new_thread->ctx.ss = 0x10;

        stack_vma = (uintptr_t)stack + MEM_PHYS_OFFSET;
    }

    new_thread->ctx.rsp = stack_vma + THREAD_STACK_SIZE;

    stack = (void *)stack + THREAD_STACK_SIZE + MEM_PHYS_OFFSET;

    if (want_elf) {
        // TODO
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

    cpu_local->kernel_stack = current_thread->kernel_stack;

    lapic_eoi();
    lapic_timer_oneshot(reschedule_vector, current_thread->timeslice);

    if (current_thread->ctx.cs & 0x03) {
        swapgs();
    }

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
