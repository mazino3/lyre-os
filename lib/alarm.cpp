#include <stdint.h>
#include <stddef.h>
#include <lib/alarm.hpp>
#include <sys/idt.hpp>
#include <sys/apic.hpp>
#include <lib/print.hpp>

static uint8_t alarm_vector;

__attribute__((interrupt))
static void alarm_interrupt_handler(void *) {
    lapic_eoi();
}

void alarm_init() {
    alarm_vector = idt_get_empty_int_vector();

    idt_register_interrupt_handler(alarm_vector, (void*)alarm_interrupt_handler, 0, 0x8e);
}
