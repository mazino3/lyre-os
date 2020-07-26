#include <mm/memmap.hpp>
#include <stivale.hpp>

static Stivale *stivale_info;

void memmap_init(struct Stivale *info) {
	stivale_info = info;
}

uint64_t memmap_get_entries_count() {
    return stivale_info->memmap.entries;
}

void memmap_get_entry(struct MemmapEntry *buf, uint64_t index) {
    StivaleMemmapEntry *entry = stivale_info->memmap.address + index;
    buf->base = entry->base;
    buf->size = entry->size;
    buf->type = (MemmapEntryType)(entry->type);
}
