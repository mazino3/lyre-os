#pragma once

#include <stddef.h>

class Bitmap {

private:
    void *bitmap;

public:
    Bitmap(void *bitmap) {
        this->bitmap = bitmap;
    }

    bool is_set(size_t bit) {
        bool ret;
        asm volatile (
            "bt %1, (%2)"
            : "=@ccc" (ret)
            : "r" (bit), "r" (bitmap)
            : "memory"
        );
        return ret;
    }

    bool set(size_t bit) {
        bool ret;
        asm volatile (
            "bts %1, (%2)"
            : "=@ccc" (ret)
            : "r" (bit), "r" (bitmap)
            : "memory"
        );
        return ret;
    }

    bool unset(size_t bit) {
        bool ret;
        asm volatile (
            "btr %1, (%2)"
            : "=@ccc" (ret)
            : "r" (bit), "r" (bitmap)
            : "memory"
        );
        return ret;
    }

};
