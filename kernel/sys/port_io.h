#pragma once

#include <stdint.h>

static inline void outb(uint16_t port, uint8_t value) {
    asm volatile (
        "out %0, %1\n\t"
        :
        : "d"(port), "a"(value)
        : "memory"
    );
}

static inline void outw(uint16_t port, uint16_t value) {
    asm volatile (
        "out %0, %1\n\t"
        :
        : "d"(port), "a"(value)
        : "memory"
    );
}

static inline void outd(uint16_t port, uint32_t value) {
    asm volatile (
        "out %0, %1\n\t"
        :
        : "d"(port), "a"(value)
        : "memory"
    );
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    asm volatile (
        "in %0, %1\n\t"
        : "=a"(ret)
        : "d"(port)
        : "memory"
    );
    return ret;
}

static inline uint16_t inw(uint16_t port) {
    uint16_t ret;
    asm volatile (
        "in %0, %1\n\t"
        : "=a"(ret)
        : "d"(port)
        : "memory"
    );
    return ret;
}

static inline uint32_t ind(uint16_t port) {
    uint32_t ret;
    asm volatile (
        "in %0, %1\n\t"
        : "=a"(ret)
        : "d"(port)
        : "memory"
    );
    return ret;
}
