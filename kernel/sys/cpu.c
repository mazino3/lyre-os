#include <stddef.h>
#include <stdint.h>
#include <sys/cpu.h>
#include <sys/apic.h>
#include <lib/print.h>
#include <sys/hpet.h>
#include <stivale/stivale2.h>
#include <lib/lock.h>
#include <lib/alloc.h>
#include <mm/pmm.h>
#include <mm/vmm.h>
#include <sys/gdt.h>
#include <sys/idt.h>
#include <sched/sched.h>

struct cpu_local *cpu_locals;

static void cpu_init(struct stivale2_smp_info *smp_info);

static uint32_t bsp_lapic_id;
static size_t   cpus_online = 0;

void smp_init(struct stivale2_struct_tag_smp *smp_tag) {
    print("smp: BSP LAPIC ID:    %x\n", smp_tag->bsp_lapic_id);
    print("smp: Total CPU count: %U\n", smp_tag->cpu_count);

    bsp_lapic_id = smp_tag->bsp_lapic_id;

    cpu_locals = alloc(sizeof(struct cpu_local) * smp_tag->cpu_count);

    for (size_t i = 0; i < smp_tag->cpu_count; i++) {
        LOCKED_WRITE(smp_tag->smp_info[i].extra_argument, (uint64_t)&cpu_locals[i]);
        uint64_t stack = (uintptr_t)pmm_allocz(1) + MEM_PHYS_OFFSET;
        uint64_t sched_stack = (uintptr_t)pmm_allocz(1) + MEM_PHYS_OFFSET;
        cpu_locals[i].tss.rsp0 = stack;
        cpu_locals[i].tss.ist1 = sched_stack;
        if (smp_tag->smp_info[i].lapic_id == bsp_lapic_id) {
            cpu_init(((void *)&smp_tag->smp_info[i]) - MEM_PHYS_OFFSET);
            continue;
        }
        cpu_locals[i].cpu_number = i;
        LOCKED_WRITE(smp_tag->smp_info[i].target_stack, stack);
        LOCKED_WRITE(smp_tag->smp_info[i].goto_address, (uint64_t)cpu_init);
    }

    while (LOCKED_READ(cpus_online) != smp_tag->cpu_count);

    print("smp: All CPUs online\n");
}

#define MAX_TSC_CALIBRATIONS 4

extern void syscall_entry(void);

void syscall_set_fs_base(struct cpu_gpr_context *ctx) {
    uintptr_t ptr = (uintptr_t)ctx->rdi;

    set_user_fs(ptr);
}

static void cpu_init(struct stivale2_smp_info *smp_info) {
    smp_info = (void *)smp_info + MEM_PHYS_OFFSET;

    gdt_reload();
    idt_reload();
    vmm_switch_pagemap(kernel_pagemap);

    // Load CPU local address in gsbase
    set_kernel_gs((uintptr_t)smp_info->extra_argument);
    set_user_gs((uintptr_t)smp_info->extra_argument);

    print("smp: Processor #%u launched\n", this_cpu->cpu_number);

    gdt_load_tss((uintptr_t)&this_cpu->tss);

    this_cpu->lapic_id = smp_info->lapic_id;

    // First enable SSE/SSE2 as it is baseline for x86_64
    uint64_t cr0 = 0;
    cr0 = read_cr("0");
    cr0 &= ~(1 << 2);
    cr0 |=  (1 << 1);
    write_cr("0", cr0);

    uint64_t cr4 = 0;
    cr4 = read_cr("4");
    cr4 |= (3 << 9);
    write_cr("4", cr4);

    // Initialise the PAT
    uint64_t pat_msr = rdmsr(0x277);
    pat_msr &= 0xffffffff;
    // write-protect / write-combining
    pat_msr |= (uint64_t)0x0105 << 32;
    wrmsr(0x277, pat_msr);

    // Enable syscall in EFER
    uint64_t efer = rdmsr(0xc0000080);
    efer |= 1;
    wrmsr(0xc0000080, efer);

    // Set up syscall
    wrmsr(0xc0000081, 0x0013000800000000);
    // Syscall entry address
    wrmsr(0xc0000082, (uint64_t)syscall_entry);
    // Flags mask
    wrmsr(0xc0000084, (uint64_t)~((uint32_t)0x002));

    uint32_t a, b, c, d;
    cpuid(1, 0, &a, &b, &c, &d);

    if ((c & CPUID_XSAVE)) {
        cr4 = read_cr("4");
        cr4 |= (1 << 18); // Enable XSAVE and x{get, set}bv
        write_cr("4", cr4);

        uint64_t xcr0 = 0;
        xcr0 |= (1 << 0); // Save x87 state with xsave
        xcr0 |= (1 << 1); // Save SSE state with xsave

        if ((c & CPUID_AVX))
            xcr0 |= (1 << 2); // Enable AVX and save AVX state with xsave

        if (cpuid(7, 0, &a, &b, &c, &d)) {
            if ((b & CPUID_AVX512)) {
                xcr0 |= (1 << 5); // Enable AVX-512
                xcr0 |= (1 << 6); // Enable management of ZMM{0 -> 15}
                xcr0 |= (1 << 7); // Enable management of ZMM{16 -> 31}
            }
        }
        wrxcr(0, xcr0);

        this_cpu->fpu_storage_size = (size_t)c;

        this_cpu->fpu_save = xsave;
        this_cpu->fpu_restore = xrstor;
    } else {
        this_cpu->fpu_storage_size = 512; // Legacy size for fxsave
        this_cpu->fpu_save = fxsave;
        this_cpu->fpu_restore = fxrstor;
    }

    cpuid(0x1, 0, &a, &b, &c, &d);
    if (!(c & CPUID_TSC_DEADLINE)) {
        print("cpu: No TSC-deadline mode!!!\n");
        //for (;;);
    }
    // Check for invariant TSC
    cpuid(0x80000007, 0, &a, &b, &c, &d);
    if (!(d & CPUID_INVARIANT_TSC)) {
        print("cpu: No invariant TSC!!!\n");
        //for (;;);
    }

    // Calibrate the TSC
    for (int i = 0; i < MAX_TSC_CALIBRATIONS; i++) {
        uint64_t initial_tsc_reading = rdtsc();

        // Wait 1 millisecond
        hpet_usleep(1000);

        uint64_t final_tsc_reading = rdtsc();

        uint64_t freq = (final_tsc_reading - initial_tsc_reading) * 1000;
        print("cpu: TSC reading #%u yielded a frequency of %U Hz.\n", i, freq);

        this_cpu->tsc_frequency += freq;
    }

    // Average out all readings
    this_cpu->tsc_frequency /= MAX_TSC_CALIBRATIONS;
    print("cpu: TSC frequency fixed at %U Hz.\n", this_cpu->tsc_frequency);

    lapic_enable(0xff);

    LOCKED_INC(cpus_online);

    if (this_cpu->lapic_id != bsp_lapic_id) {
        sched_wait();
    }
}
