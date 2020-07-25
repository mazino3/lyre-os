#pragma once

#include <stdint.h>

enum MemmapEntryType : uint32_t {
    MMAP_USABLE      = 1,
    MMAP_RESERVED    = 2,
    MMAP_ACPIRECLAIM = 3,
    MMAP_ACPINVS     = 4,
    MMAP_KERNEL      = 10
};

struct MemmapEntry {
    uint64_t base;
    uint64_t size;
    MemmapEntryType type;
};

uint64_t arch_mmap_get_entries_count();
void arch_mmap_get_entry(struct MemmapEntry* buf, uint64_t index);