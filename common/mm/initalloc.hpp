#pragma once
#include <stddef.h>
#include <stdint.h>

void *arch_init_alloc(size_t size);
uint64_t arch_get_phys_brk();