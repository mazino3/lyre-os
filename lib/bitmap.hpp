#pragma once

#include <stddef.h>
#include <lib/asm.hpp>

class Bitmap {

private:
    void *bitmap;

public:
    void set_bitmap(void *bitmap) {
        this->bitmap = bitmap;
    }

    bool is_set(size_t bit) {
        bool ret;
        asm volatile (
            "bt %1, %2"
            : "=@ccc" (ret)
            : "m" (FLAT_PTR(bitmap)), "r" (bit)
            : "memory"
        );
        return ret;
    }

    bool set(size_t bit) {
        bool ret;
        asm volatile (
            "bts %1, %2"
            : "=@ccc" (ret), "+m" (FLAT_PTR(bitmap))
            : "r" (bit)
            : "memory"
        );
        return ret;
    }

    bool unset(size_t bit) {
        bool ret;
        asm volatile (
            "btr %1, %2"
            : "=@ccc" (ret), "+m" (FLAT_PTR(bitmap))
            : "r" (bit)
            : "memory"
        );
        return ret;
    }

};
