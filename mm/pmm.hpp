#pragma once

#include <stddef.h>
#include <lib/stivale.hpp>

void pmm_init(StivaleMemmap memmap);
void *pmm_alloc(size_t count);
void *pmm_allocz(size_t count);
void pmm_free(void *ptr, size_t count);
