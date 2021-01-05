#include <stdbool.h>
#include <lib/dmesg.h>
#include <lib/lock.h>
#include <lib/alloc.h>
#include <lib/builtins.h>

typeof(dmesg) dmesg = NULL;

static ssize_t dmesg_write(typeof(dmesg) this, const void *buf, size_t size) {
    if (!this->enabled)
        return -1;

    SPINLOCK_ACQUIRE(this->lock);

    while (this->ptr + size > this->buf_size) {
        this->buf_size += 128;
        this->buffer    = realloc(this->buffer, this->buf_size);
    }

    memcpy(this->buffer + this->ptr, buf, size);

    this->ptr += size;

    LOCK_RELEASE(this->lock);
    return size;
}

static void dmesg_init(void) {
    dmesg = resource_create(sizeof(dmesg));

    dmesg->enabled  = false;
    dmesg->buf_size = 0;
    dmesg->ptr      = 0;
    dmesg->buffer   = NULL;

    dmesg->write = (void *)dmesg_write;
}

void dmesg_enable(void) {
    if (dmesg == NULL)
        dmesg_init();
    dmesg->enabled = true;
}

void dmesg_disable(void) {
    dmesg->enabled = false;
}
