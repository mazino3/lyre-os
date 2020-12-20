#pragma once

#include <stddef.h>
#include <stdbool.h>
#include <lib/handle.h>
#include <lib/types.h>
#include <lib/lock.h>

void dmesg_enable();
void dmesg_disable();

extern struct {
    struct handle;
    bool    enabled;
    size_t  buf_size;
    size_t  ptr;
    char   *buffer;
    lock_t  lock;
} *dmesg;
