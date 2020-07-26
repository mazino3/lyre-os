#pragma once

#include <lib/debug.hpp>
#include <lib/port_io.hpp>

static inline void debug_log(const char *msg) {
    while (*msg) {
        outb(0xe9, *msg);
        msg++;
    }
}
