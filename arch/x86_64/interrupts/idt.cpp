#include <stddef.h>
#include <stdint.h>
#include <interrupts/idt.hpp>
#include <lib/lock.hpp>

static Lock idt_lock;
static int free_int_vect_base = 0x80;
static const int free_int_vect_limit = 0xa0;

int idt_get_empty_int_vector() {
    idt_lock.acquire();

    int ret;
    if (free_int_vect_base == free_int_vect_limit)
        ret = -1;
    else
        ret = free_int_vect_base++;

    idt_lock.release();
    return ret;
}

struct IDTEntry {
    uint16_t offset_lo;
    uint16_t selector;
    uint8_t  ist;
    uint8_t  type_attr;
    uint16_t offset_mid;
    uint32_t offset_hi;
    uint32_t zero;
} __attribute((packed));

struct IDTPtr {
    uint16_t size;
    uint64_t address;
} __attribute((packed));

static struct IDTEntry idt[256];
extern void *int_thunks[];

static void register_interrupt_handler(size_t vec, void *handler, uint8_t ist, uint8_t type) {
    uint64_t p = (uint64_t)handler;

    idt[vec].offset_lo = (uint16_t)p;
    idt[vec].selector = 0x08;
    idt[vec].ist = ist;
    idt[vec].type_attr = type;
    idt[vec].offset_mid = (uint16_t)(p >> 16);
    idt[vec].offset_hi = (uint32_t)(p >> 32);
    idt[vec].zero = 0;
}

void idt_init() {
    idt_lock.acquire();

    /* Register all interrupts to thunks */
    for (size_t i = 0; i < 256; i++)
        register_interrupt_handler(i, int_thunks[i], 0, 0x8e);

    IDTPtr idt_ptr = {
        sizeof(idt) - 1,
        (uint64_t)idt
    };

    asm volatile (
        "lidt %0"
        :
        : "m" (idt_ptr)
    );

    idt_lock.release();
}
