#pragma once

#include <stddef.h>
#include <limine/stivale/stivale.h>

void pmm_init(struct stivale_mmap_entry *memmap, size_t memmap_entries);
void *pmm_alloc(size_t count);
void *pmm_allocz(size_t count);
void pmm_free(void *ptr, size_t count);
