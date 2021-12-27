#ifndef __SCHED__SCHED_H__
#define __SCHED__SCHED_H__

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <lib/types.h>
#include <lib/dynarray.h>
#include <lib/lock.h>
#include <lib/elf.h>
#include <lib/event.h>
#include <sys/cpu.h>
#include <mm/vmm.h>
#include <fs/vfs.h>

struct process;

struct thread {
    tid_t tid;
    lock_t lock;
    bool is_queued;
    size_t timeslice;
    struct cpu_gpr_context ctx;
    struct process *process;
    uintptr_t user_gs;
    uintptr_t user_fs;
    uintptr_t user_stack;
    uintptr_t kernel_stack;
    uintptr_t page_fault_stack;
    lock_t event_block_dequeue;
    lock_t event_occurred;
    size_t which_event;
};

struct process {
    pid_t pid;
    pid_t ppid;
    struct pagemap *pagemap;
    DYNARRAY_STRUCT(struct thread *) threads;
    uintptr_t thread_stack_top;
    uintptr_t mmap_anon_non_fixed_base;
    DYNARRAY_STRUCT(struct file_descriptor *) fds;
    struct vfs_node *current_directory;
    struct event *event;
    int status;
    DYNARRAY_STRUCT(struct process *) children;
};

extern struct process *kernel_process;

void sched_init(void);

__attribute__((noreturn))
void sched_wait(void);

struct process *sched_start_program(bool execve,
                                    const char *path,
                                    const char **argv,
                                    const char **envp,
                                    const char *stdin,
                                    const char *stdout,
                                    const char *stderr);

struct process *sched_new_process(struct process *old_process, struct pagemap *pagemap);

struct thread *sched_new_thread(struct thread *new_thread,
                                struct process *proc,
                                bool want_elf,
                                void *addr,
                                void *arg,
                                const char **argv,
                                const char **envp,
                                struct auxval_t *auxval,
                                bool start,
                                struct pagemap *new_pagemap);

__attribute__((noreturn))
void sched_spinup(struct cpu_gpr_context *);

void sched_yield(void);
bool sched_queue(struct thread *thread);
bool sched_dequeue(struct thread *thread);
void sched_dequeue_and_yield(void);
__attribute__((noreturn)) void sched_dequeue_and_die(void);

#endif
