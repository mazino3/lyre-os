#include <lib/handle.hpp>
#include <lib/types.hpp>

// These functions should be stubs for generic kernel handles unused functions.

ssize_t Handle::read(void *, size_t) {
    return -1;
}

ssize_t Handle::write(const void *, size_t) {
    return -1;
}
