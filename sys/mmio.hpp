#pragma once

#include <stdint.h>

static inline void mmoutb(void *addr, uint8_t value) {
    asm volatile (
        "mov %0, %1\n\t"
        : "=m"(addr)
        : "r"(value)
        : "memory"
    );
}

static inline void mmoutw(void *addr, uint16_t value) {
    asm volatile (
        "mov %0, %1\n\t"
        : "=m"(addr)
        : "r"(value)
        : "memory"
    );
}

static inline void mmoutd(void *addr, uint32_t value) {
    asm volatile (
        "mov %0, %1\n\t"
        : "=m"(addr)
        : "r"(value)
        : "memory"
    );
}

static inline void mmoutq(void *addr, uint64_t value) {
    asm volatile (
        "mov %0, %1\n\t"
        : "=m"(addr)
        : "r"(value)
        : "memory"
    );
}

static inline uint8_t mminb(void *addr) {
    uint8_t ret;
    asm volatile (
        "mov %0, %1\n\t"
        : "=r"(ret)
        : "m"(addr)
        : "memory"
    );
    return ret;
}

static inline uint16_t mminw(void *addr) {
    uint16_t ret;
    asm volatile (
        "mov %0, %1\n\t"
        : "=r"(ret)
        : "m"(addr)
        : "memory"
    );
    return ret;
}

static inline uint32_t mmind(void *addr) {
    uint32_t ret;
    asm volatile (
        "mov %0, %1\n\t"
        : "=r"(ret)
        : "m"(addr)
        : "memory"
    );
    return ret;
}

static inline uint64_t mminq(void *addr) {
    uint64_t ret;
    asm volatile (
        "mov %0, %1\n\t"
        : "=r"(ret)
        : "m"(addr)
        : "memory"
    );
    return ret;
}
