#pragma once

#include <stddef.h>

template <class T> class DynArray {
    size_t count, capacity;
    char *data;

    size_t calculate_capacity(size_t size) {
        size_t result = 1;
        while (size < result) {
            result *= 2;
        }
        return result;
    }

    bool relocate(size_t new_capacity) {
        char *new_data = new char[sizeof(T) * new_capacity];
        if (new_data == nullptr) {
            return false;
        }
        if (data != nullptr) {
            memcpy(new_data, data, sizeof(T) * count);
        }
        capacity = new_capacity;
        data = new_data;
        return true;
    }

public:
    DynArray() {
        count = 0;
        capacity = 0;
        data = nullptr;
    }

    bool push_back(const T& elem) {
        if (count == capacity) {
            if (!relocate(calculate_capacity(count + 1))) {
                return false;
            }
            ((T*)(data))[count] = elem;
            count++;
        }
        return true;
    }

    bool pop_back() {
        if (count == 0) {
            return false;
        }
        size_t new_capacity = calculate_capacity(--count);
        if (new_capacity != capacity) {
            if(!relocate(new_capacity)) {
                return false;
            }
        }
        return true;
    }

    size_t size() { return size; }

    T& operator[](size_t ind) {
        return ((T*)data)[ind];
    }
};