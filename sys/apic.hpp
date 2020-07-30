#pragma once

#include <stdint.h>
#include <stddef.h>

extern uint32_t *lapic_eoi_ptr;

void lapic_enable(uint8_t spurious_vector);
void lapic_eoi(void);
void lapic_send_ipi(int cpu, uint8_t vector);

void io_apic_set_gsi_redirect(uint8_t lapic_id, uint8_t vec, uint32_t gsi, uint16_t flags, bool status);
void io_apic_set_irq_redirect(uint8_t lapic_id, uint8_t vec, uint8_t irq, bool status);

void apic_init();
