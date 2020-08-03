#pragma once

#include <stdint.h>
#include <stddef.h>
#include <lib/asm.hpp>

extern uint64_t cpu_tsc_frequency;
extern size_t cpu_fpu_storage_size;

extern void (*cpu_fpu_save)(void *);
extern void (*cpu_fpu_restore)(void *);

void cpu_init();

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
