#pragma once

#include <stddef.h>
#include <lib/alloc.hpp>
#include <lib/builtins.h>

template <typename T> class DynArray {
    T*     storage;
    size_t size;
    size_t count;

    void grow() {
        size   *= 2;
        storage = (T*)realloc(storage, size * sizeof(T));
    }

public:
    DynArray(size_t initial_size = 0) {
        storage = (T*)alloc(initial_size * sizeof(T));
        size    = initial_size;
        count   = 0;
    }

    ~DynArray() {
        free(storage);
    }

    size_t length() {
        return count;
    }

    size_t push_back(T elem) {
        if (count >= size)
            grow();

        storage[count] = elem;
        return count++;
    }

    void pop_back() {
        if (count != 0)
            count--;
    }

    inline T& operator[](size_t ind) {
        return storage[ind];
    }

    void shrink_to_fit() {
        storage = (T*)realloc(storage, count * sizeof(T));
        size    = count;
    }
};
