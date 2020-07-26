#pragma once

class Lock {

private:
    uint8_t lock = 0;

public:
    inline void acquire() {
        asm volatile (
            "1: lock bts $0, (%0)\n\t"
            "jnc 1f\n\t"
            "pause\n\t"
            "jmp 1b\n\t"
            "1:\n\t"
            :
            : "r" (&this->lock)
            : "memory"
        );
    }

    inline bool test_and_acquire() {
        bool ret;
        asm volatile (
            "lock bts $0, (%1)\n\t"
            : "=@ccc" (ret)
            : "r" (&this->lock)
            : "memory"
        );
        return ret;
    }

    inline void release() {
        asm volatile (
            "lock btr $0, (%0)\n\t"
            :
            : "r" (&this->lock)
            : "memory"
        );
    }
};
