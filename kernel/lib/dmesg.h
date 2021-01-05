#pragma once

#include <stddef.h>
#include <stdbool.h>
#include <lib/resource.h>
#include <lib/types.h>
#include <lib/lock.h>

void dmesg_enable();
void dmesg_disable();

extern struct {
    struct resource;
    bool    enabled;
    size_t  buf_size;
    size_t  ptr;
    char   *buffer;
} *dmesg;
