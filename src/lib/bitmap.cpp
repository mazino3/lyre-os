#include <lib/bitmap.hpp>

Bitmap::Bitmap(void *bitmap) {
    Bitmap::bitmap = bitmap;
}

bool Bitmap::is_set(size_t bit) {
    bool ret;
    asm volatile (
        "bt %1, (%2)"
        : "=@ccc" (ret)
        : "r" (bit), "r" (bitmap)
        : "memory"
    );
    return ret;
}

bool Bitmap::set(size_t bit) {
    bool ret;
    asm volatile (
        "bts %1, (%2)"
        : "=@ccc" (ret)
        : "r" (bit), "r" (bitmap)
        : "memory"
    );
    return ret;
}

bool Bitmap::unset(size_t bit) {
    bool ret;
    asm volatile (
        "btr %1, (%2)"
        : "=@ccc" (ret)
        : "r" (bit), "r" (bitmap)
        : "memory"
    );
    return ret;
}
