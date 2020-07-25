#pragma once

#include <stddef.h>

class Bitmap {
private:
    void *bitmap;
public:
    Bitmap(void *bitmap);
    bool is_set(size_t bit);
    bool set(size_t bit);
    bool unset(size_t bit);
};
