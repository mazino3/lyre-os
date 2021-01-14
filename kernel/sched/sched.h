#ifndef __SCHED__SCHED_H__
#define __SCHED__SCHED_H__

#include <stddef.h>
#include <stdint.h>
#include <lib/types.h>
#include <lib/dynarray.h>
#include <sys/cpu.h>
#include <mm/vmm.h>

struct process;

struct thread {
    tid_t tid;
    struct cpu_gpr_context ctx;
    struct process *process;
};

struct process {
    pid_t pid;
    struct pagemap *pagemap;
    DYNARRAY_STRUCT(struct thread *) threads;
    uintptr_t thread_stack_top;
};

#endif
