#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

extern uint64_t lapic_timer_frequency;
extern uint32_t *lapic_eoi_ptr;

void lapic_enable(uint8_t spurious_vector);
void lapic_eoi(void);
void lapic_send_ipi(uint8_t lapic_id, uint8_t vector);

void io_apic_set_gsi_redirect(uint8_t lapic_id, uint8_t vec, uint32_t gsi, uint16_t flags, bool status);
void io_apic_set_irq_redirect(uint8_t lapic_id, uint8_t vec, uint8_t irq, bool status);

void lapic_timer_oneshot(uint8_t vector, uint64_t ms);

void apic_init(void);
