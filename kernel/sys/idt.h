#pragma once

#include <stdint.h>
#include <stddef.h>

void idt_init();
int idt_get_empty_int_vector();
void idt_register_interrupt_handler(size_t vec, void *handler, uint8_t ist, uint8_t type);
void idt_reload(void);
