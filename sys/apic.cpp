#include <stdint.h>
#include <stddef.h>
#include <sys/apic.hpp>
#include <sys/hpet.hpp>
#include <acpi/madt.hpp>
#include <mm/vmm.hpp>
#include <sys/mmio.hpp>
#include <sys/cpu.hpp>
#include <lib/print.hpp>
#include <lib/types.hpp>

#define LAPIC_REG_ICR0     0x300
#define LAPIC_REG_ICR1     0x310
#define LAPIC_REG_SPURIOUS 0x0f0
#define LAPIC_REG_EOI      0x0b0

#define LAPIC_REG_TIMER         0x320
#define LAPIC_REG_TIMER_INITCNT 0x380
#define LAPIC_REG_TIMER_CURCNT  0x390
#define LAPIC_REG_TIMER_DIV     0X3e0

static uint8_t *lapic_mmio_base;
uint32_t *lapic_eoi_ptr;

static uint32_t lapic_read(uint32_t reg) {
    return mmind(lapic_mmio_base + reg);
}

static void lapic_write(uint32_t reg, uint32_t data) {
    mmoutd(lapic_mmio_base + reg, data);
}

void lapic_enable(uint8_t spurious_vector) {
    // Spurious vector must end in 0xf (TODO: might wanna assert here)
    // Write the spurious vector ORd with bit 8, aka LAPIC enable.
    lapic_write(LAPIC_REG_SPURIOUS,
                lapic_read(LAPIC_REG_SPURIOUS) | (1 << 8) | spurious_vector);
}

void lapic_eoi(void) {
    // Writing 0 to this register signals EOI. Any other value might #GP.
    lapic_write(LAPIC_REG_EOI, 0);
}

void lapic_send_ipi(uint8_t lapic_id, uint8_t vector) {
    lapic_write(LAPIC_REG_ICR1, ((uint32_t)lapic_id) << 24);
    lapic_write(LAPIC_REG_ICR0, vector);
}

static uint32_t io_apic_read(size_t io_apic_num, uint32_t reg) {
    uint8_t *base = (uint8_t *)(uintptr_t)madt_io_apics[io_apic_num]->addr + MEM_PHYS_OFFSET;
    mmoutd(base, reg);
    return mmind(base + 4);
}

static void io_apic_write(size_t io_apic_num, uint32_t reg, uint32_t data) {
    uint8_t *base = (uint8_t *)(uintptr_t)madt_io_apics[io_apic_num]->addr + MEM_PHYS_OFFSET;
    mmoutd(base, reg);
    mmoutd(base + 4, data);
}

// Get the maximum number of redirects this I/O APIC can handle
static uint32_t io_apic_get_max_redirect(size_t io_apic_num) {
    return (io_apic_read(io_apic_num, 1) & 0xff0000) >> 16;
}

// Return the index of the I/O APIC that handles this redirect
static ssize_t io_apic_from_redirect(uint32_t gsi) {
    for (size_t i = 0; i < madt_io_apics.length(); i++) {
        if (madt_io_apics[i]->gsib <= gsi && madt_io_apics[i]->gsib + io_apic_get_max_redirect(i) > gsi)
            return i;
    }

    return -1;
}

void io_apic_set_gsi_redirect(uint8_t lapic_id, uint8_t vec, uint32_t gsi, uint16_t flags, bool status) {
    size_t io_apic = io_apic_from_redirect(gsi);

    uint64_t redirect = vec;

    // Active high(0) or low(1)
    if (flags & 2) {
        redirect |= (1 << 13);
    }

    // Edge(0) or level(1) triggered
    if (flags & 8) {
        redirect |= (1 << 15);
    }

    if (!status) {
        // Set mask bit
        redirect |= (1 << 16);
    }

    // Set target APIC ID
    redirect |= ((uint64_t)lapic_id) << 56;
    uint32_t ioredtbl = (gsi - madt_io_apics[io_apic]->gsib) * 2 + 16;

    io_apic_write(io_apic, ioredtbl + 0, (uint32_t)redirect);
    io_apic_write(io_apic, ioredtbl + 1, (uint32_t)(redirect >> 32));
}

void io_apic_set_irq_redirect(uint8_t lapic_id, uint8_t vec, uint8_t irq, bool status) {
    for (size_t i = 0; i < madt_isos.length(); i++) {
        if (madt_isos[i]->irq_source == irq) {
            print("apic: IRQ %u used by override.\n", irq);
            io_apic_set_gsi_redirect(lapic_id, vec, madt_isos[i]->gsi,
                                     madt_isos[i]->flags, status);
            return;
        }
    }
    io_apic_set_gsi_redirect(lapic_id, vec, irq, 0, status);
}

void lapic_timer_oneshot(uint8_t vector, uint64_t us) {
    // Use TSC-deadline mode, set vector
    lapic_write(LAPIC_REG_TIMER, (0b10 << 17) | vector);

    uint64_t ticks  = us * (cpu_tsc_frequency / 1000000);
    uint64_t target = rdtsc() + ticks;

    wrmsr(IA32_TSC_DEADLINE, target);
}

void apic_init() {
    lapic_mmio_base = (uint8_t *)(uintptr_t)madt->local_controller_addr + MEM_PHYS_OFFSET;
    lapic_eoi_ptr = (uint32_t *)(lapic_mmio_base + LAPIC_REG_EOI);
    lapic_enable(0xff);

    print("apic: Init done.\n");
}
