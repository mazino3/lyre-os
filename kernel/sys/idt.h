#ifndef __SYS__IDT_H__
#define __SYS__IDT_H__

#include <stdint.h>
#include <stddef.h>
#include <lib/event.h>

extern struct event *int_event[256];

void idt_init(void);
int idt_get_empty_int_vector(void);
void idt_register_interrupt_handler(size_t vec, void *handler, uint8_t ist, uint8_t type);
void idt_reload(void);

#endif
