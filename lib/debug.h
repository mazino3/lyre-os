#pragma once

#include <lib/debug.h>
#include <sys/port_io.h>

static inline void debug_log(const char *msg) {
    while (*msg) {
        outb(0xe9, *msg);
        msg++;
    }
}
