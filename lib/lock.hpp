#pragma once

#include <stdint.h>

class Lock {

private:
    uint32_t lock = 0;

public:
    inline void acquire() {
        asm volatile (
            "1: lock btsd %0, 0\n\t"
            "jnc 1f\n\t"
            "pause\n\t"
            "jmp 1b\n\t"
            "1:\n\t"
            : "+m" (this->lock)
            :
            : "memory"
        );
    }

    inline bool test_and_acquire() {
        bool ret;
        asm volatile (
            "lock btsd %0, 0\n\t"
            : "+m" (this->lock), "=@ccc" (ret)
            :
            : "memory"
        );
        return ret;
    }

    inline void release() {
        asm volatile (
            "lock btrd %0, 0\n\t"
            : "+m" (this->lock)
            :
            : "memory"
        );
    }
};
