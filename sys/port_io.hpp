#pragma once

#include <stdint.h>

static inline void outb(uint16_t port, uint8_t value) {
    asm volatile (
        "outb %0, %1\n\t"
        :
        : "a"(value), "d"(port)
        : "memory"
    );
}
