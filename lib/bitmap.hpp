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
            "bt [%2], %1"
            : "=@ccc" (ret)
            : "r" (bit), "r" (bitmap)
            : "memory"
        );
        return ret;
    }

    bool set(size_t bit) {
        bool ret;
        asm volatile (
            "bts [%2], %1"
            : "=@ccc" (ret)
            : "r" (bit), "r" (bitmap)
            : "memory"
        );
        return ret;
    }

    bool unset(size_t bit) {
        bool ret;
        asm volatile (
            "btr [%2], %1"
            : "=@ccc" (ret)
            : "r" (bit), "r" (bitmap)
            : "memory"
        );
        return ret;
    }

};
