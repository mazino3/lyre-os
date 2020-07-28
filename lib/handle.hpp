#pragma once

#include <lib/types.hpp>

// This is the base class for all kernel handles.

class Handle {

public:
    virtual ssize_t read(void *buf, size_t count);
    virtual ssize_t write(const void *buf, size_t count);

};
