#include <stddef.h>
#include <stdint.h>
#include <sys/idt.h>
#include <lib/lock.h>
#include <lib/event.h>
#include <sys/apic.h>

static lock_t idt_lock;
static int free_int_vect_base = 0x80;
static const int free_int_vect_limit = 0xa0;

int idt_get_empty_int_vector(void) {
    SPINLOCK_ACQUIRE(idt_lock);

    int ret;
    if (free_int_vect_base == free_int_vect_limit)
        ret = -1;
    else
        ret = free_int_vect_base++;

    LOCK_RELEASE(idt_lock);
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

void idt_register_interrupt_handler(size_t vec, void *handler, uint8_t ist, uint8_t type) {
    uint64_t p = (uint64_t)handler;

    idt[vec].offset_lo = (uint16_t)p;
    idt[vec].selector = 0x08;
    idt[vec].ist = ist;
    idt[vec].type_attr = type;
    idt[vec].offset_mid = (uint16_t)(p >> 16);
    idt[vec].offset_hi = (uint32_t)(p >> 32);
    idt[vec].zero = 0;
}

struct event *int_event[256];

void idt_raise_int_event(size_t i) {
    event_trigger(int_event[i]);
    lapic_eoi();
}

void idt_init(void) {
    SPINLOCK_ACQUIRE(idt_lock);

    /* Register all interrupts to thunks */
    for (size_t i = 0; i < 256; i++) {
        int_event[i] = event_create(16);
        idt_register_interrupt_handler(i, int_thunks[i], 0, 0x8e);
    }

    idt_reload();

    LOCK_RELEASE(idt_lock);
}

void idt_reload(void) {
    struct IDTPtr idt_ptr = {
        sizeof(idt) - 1,
        (uint64_t)idt
    };

    asm volatile (
        "lidt %0"
        :
        : "m" (idt_ptr)
    );
}
