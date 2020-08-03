#include <stddef.h>
#include <stdint.h>
#include <sys/cpu.hpp>
#include <lib/print.hpp>
#include <sys/hpet.hpp>

uint64_t cpu_tsc_frequency;

#define MAX_TSC_CALIBRATIONS 4

size_t cpu_fpu_storage_size;

void (*cpu_fpu_save)(void *);
void (*cpu_fpu_restore)(void *);

void cpu_init() {
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

        cpu_fpu_storage_size = (size_t)c;

        cpu_fpu_save = xsave;
        cpu_fpu_restore = xrstor;
    } else {
        cpu_fpu_storage_size = 512; // Legacy size for fxsave
        cpu_fpu_save = fxsave;
        cpu_fpu_restore = fxrstor;
    }

    cpuid(0x1, 0, &a, &b, &c, &d);
    if (!(c & CPUID_TSC_DEADLINE)) {
        print("cpu: No TSC-deadline mode!!!\n");
        for (;;);
    }
    // Check for invariant TSC
    cpuid(0x80000007, 0, &a, &b, &c, &d);
    if (!(d & CPUID_INVARIANT_TSC)) {
        print("cpu: No invariant TSC!!!\n");
        for (;;);
    }

    // Calibrate the TSC
    for (int i = 0; i < MAX_TSC_CALIBRATIONS; i++) {
        uint64_t initial_tsc_reading = rdtsc();

        // Wait 1 millisecond
        hpet_usleep(1000);

        uint64_t final_tsc_reading = rdtsc();

        uint64_t freq = (final_tsc_reading - initial_tsc_reading) * 1000;
        print("cpu: TSC reading #%u yielded a frequency of %U Hz.\n", i, freq);

        cpu_tsc_frequency += freq;
    }

    // Average out all readings
    cpu_tsc_frequency /= MAX_TSC_CALIBRATIONS;
    print("cpu: TSC frequency fixed at %U Hz.\n", cpu_tsc_frequency);
}
