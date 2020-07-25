#include <gdt.hpp>
#include <mm/pmm.hpp>
#include <stivale.hpp>
#include <init.hpp>
#include <mm/mmap.hpp>

static Stivale* stivale_info;

uint64_t arch_mmap_get_entries_count() {
    return stivale_info->memmap.entries;
}

void arch_mmap_get_entry(struct MemmapEntry* buf, uint64_t index) {
    StivaleMemmapEntry* entry = stivale_info->memmap.address + index;
    buf->base = entry->base;
    buf->size = entry->size;
    buf->type = (MemmapEntryType)(entry->type);
}

extern "C" void main(Stivale *sti) {
    gdt_init();
    pmm_init(sti->memmap);
    stivale_info = sti;
    kernel_main();
}
