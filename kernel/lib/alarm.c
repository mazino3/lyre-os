#include <stdint.h>
#include <stddef.h>
#include <lib/alarm.h>
#include <sys/idt.h>
#include <sys/apic.h>
#include <lib/print.h>

static uint8_t alarm_vector;

__attribute__((interrupt))
static void alarm_interrupt_handler(void *p) {
    (void)p;
    lapic_eoi();
}

void alarm_init() {
    alarm_vector = idt_get_empty_int_vector();

    idt_register_interrupt_handler(alarm_vector, (void*)alarm_interrupt_handler, 0, 0x8e);
}
