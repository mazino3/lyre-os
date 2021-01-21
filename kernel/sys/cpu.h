#ifndef __SYS__CPU_H__
#define __SYS__CPU_H__

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <lib/asm.h>
#include <lib/types.h>
#include <stivale/stivale2.h>

struct cpu_tss {
    uint32_t unused0;
    uint64_t rsp0;
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t unused1;
    uint64_t ist1;
    uint64_t ist2;
    uint64_t ist3;
    uint64_t ist4;
    uint64_t ist5;
    uint64_t ist6;
    uint64_t ist7;
    uint64_t unused2;
    uint32_t iopb_offset;
} __attribute__((packed));

struct thread;

struct cpu_local {
    uint64_t cpu_number;
    uint64_t kernel_stack;
    uint64_t user_stack;
    int64_t errno;
    struct cpu_tss tss;
    uint32_t lapic_id;
    uint64_t tsc_frequency;
    size_t   fpu_storage_size;
    void   (*fpu_save)(void *);
    void   (*fpu_restore)(void *);
    struct thread *current_thread;
    ssize_t last_run_queue_index;
};

extern struct cpu_local *cpu_locals;

#define this_cpu ({                \
    uint64_t cpu_number;           \
    asm volatile (                 \
        "mov %0, qword ptr gs:[0]" \
        : "=r" (cpu_number)        \
        :                          \
        : "memory"                 \
    );                             \
    &cpu_locals[cpu_number];       \
})

struct cpu_gpr_context {
    uint64_t rax;
    uint64_t rbx;
    uint64_t rcx;
    uint64_t rdx;
    uint64_t rsi;
    uint64_t rdi;
    uint64_t rbp;
    uint64_t r8;
    uint64_t r9;
    uint64_t r10;
    uint64_t r11;
    uint64_t r12;
    uint64_t r13;
    uint64_t r14;
    uint64_t r15;
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
};

void smp_init(struct stivale2_struct_tag_smp *smp_tag);

#define write_cr(reg, val) \
    asm volatile ("mov cr" reg ", %0" :: "r" (val) : "memory");

#define read_cr(reg) ({ \
    size_t cr; \
    asm volatile ("mov %0, cr" reg : "=r" (cr) :: "memory"); \
    cr; \
})

static inline void invlpg(void *addr) {
    asm volatile (
        "invlpg %0"
        :
        : "m" (FLAT_PTR(addr))
        : "memory"
    );
}

#define CPUID_XSAVE         (1 << 26)
#define CPUID_AVX           (1 << 28)
#define CPUID_AVX512        (1 << 16)
#define CPUID_INVARIANT_TSC (1 << 8)
#define CPUID_TSC_DEADLINE  (1 << 24)

static inline bool cpuid(uint32_t leaf, uint32_t subleaf,
                         uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx) {
    uint32_t cpuid_max;
    asm volatile ("cpuid"
                  : "=a" (cpuid_max)
                  : "a" (leaf & 0x80000000) : "rbx", "rcx", "rdx");
    if (leaf > cpuid_max)
        return false;
    asm volatile ("cpuid"
                  : "=a" (*eax), "=b" (*ebx), "=c" (*ecx), "=d" (*edx)
                  : "a" (leaf), "c" (subleaf));
    return true;
}

static inline uint64_t rdtsc() {
    uint32_t edx, eax;
    asm volatile ("rdtsc"
                  : "=a" (eax), "=d" (edx));
    return ((uint64_t)edx << 32) | eax;
}

#define IA32_TSC_DEADLINE 0x6e0

static inline uint64_t rdmsr(uint32_t msr) {
    uint32_t edx, eax;
    asm volatile ("rdmsr"
                  : "=a" (eax), "=d" (edx)
                  : "c" (msr)
                  : "memory");
    return ((uint64_t)edx << 32) | eax;
}

static inline void wrmsr(uint32_t msr, uint64_t value) {
    uint32_t edx = value >> 32;
    uint32_t eax = (uint32_t)value;
    asm volatile ("wrmsr"
                  :
                  : "a" (eax), "d" (edx), "c" (msr)
                  : "memory");
}

static inline void wrxcr(uint32_t i, uint64_t value) {
    uint32_t edx = value >> 32;
    uint32_t eax = (uint32_t)value;
    asm volatile ("xsetbv"
                  :
                  : "a" (eax), "d" (edx), "c" (i)
                  : "memory");
}

static inline void xsave(void *region) {
    asm volatile ("xsave %0"
                  : "+m" (FLAT_PTR(region))
                  : "a" (0xffffffff), "d" (0xffffffff)
                  : "memory");
}

static inline void xrstor(void *region) {
    asm volatile ("xrstor %0"
                  :
                  : "m" (FLAT_PTR(region)), "a" (0xffffffff), "d" (0xffffffff)
                  : "memory");
}

static inline void fxsave(void *region) {
    asm volatile ("fxsave %0"
                  : "+m" (FLAT_PTR(region))
                  :
                  : "memory");
}

static inline void fxrstor(void *region) {
    asm volatile ("fxrstor %0"
                  :
                  : "m" (FLAT_PTR(region))
                  : "memory");
}

static inline void set_kernel_gs(uintptr_t addr) {
    wrmsr(0xc0000101, addr);
}

static inline void set_user_gs(uintptr_t addr) {
    wrmsr(0xc0000102, addr);
}

static inline void swapgs(void) {
    asm volatile ("swapgs" ::: "memory");
}

#endif
