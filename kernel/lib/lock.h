#pragma once

#include <stdint.h>

typedef uint32_t lock_t;

#define SPINLOCK_ACQUIRE(LOCK) ({ \
    asm volatile (                \
        "1: lock bts %0, 0\n\t"   \
        "jnc 1f\n\t"              \
        "pause\n\t"               \
        "jmp 1b\n\t"              \
        "1:"                      \
        : "+m" (LOCK)             \
        :                         \
        : "memory"                \
    );                            \
})

#define LOCK_ACQUIRE(LOCK) ({        \
    bool ret;                        \
    asm volatile (                   \
        "lock bts %0, 0"             \
        : "+m" (LOCK), "=@ccc" (ret) \
        :                            \
        : "memory"                   \
    );                               \
    ret;                             \
})

#define LOCK_RELEASE(LOCK) ({ \
    asm volatile (            \
        "lock btr %0, 0"      \
        : "+m" (LOCK)         \
        :                     \
        : "memory"            \
    );                        \
})
