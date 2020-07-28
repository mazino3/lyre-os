#include <lib/dmesg.hpp>
#include <lib/lock.hpp>
#include <lib/alloc.hpp>
#include <lib/builtins.h>

static bool dmesg_enabled = false;
static size_t buf_size = 0;
static size_t ptr = 0;
static char *buffer = nullptr;
static Lock lock;

void dmesg_enable() {
    dmesg_enabled = true;
}

void dmesg_disable() {
    dmesg_enabled = false;
}

ssize_t DMesg::write(const void *buf, size_t size) {
    if (!dmesg_enabled)
        return -1;

    lock.acquire();

    while (ptr + size > buf_size) {
        buf_size += 128;
        buffer = (char *)realloc(buffer, buf_size);
    }

    memcpy(buffer + ptr, buf, size);

    ptr += size;

    lock.release();
    return size;
}
