#include <stdint.h>
#include <stddef.h>
#include <mm/pmm.hpp>
#include <mm/vmm.hpp>
#include <lib/bitmap.hpp>
#include <lib/math.hpp>

static Bitmap bitmap(nullptr);
static size_t last_used_index = 0;
static uintptr_t highest_page = 0;

void pmm_init(StivaleMemmap memmap) {
    // First, calculate how big the bitmap needs to be.
    for (size_t i = 0; i < memmap.entries; i++) {
        if (memmap.address[i].type != USABLE)
            continue;

        uintptr_t top = memmap.address[i].base + memmap.address[i].size;

        if (top > highest_page)
            highest_page = top;
    }

    size_t bitmap_size = div_roundup(highest_page, PAGE_SIZE) / 8;

    // Second, find a location with enough free pages to host the bitmap.
    for (size_t i = 0; i < memmap.entries; i++) {
        if (memmap.address[i].type != USABLE)
            continue;

        if (memmap.address[i].size >= bitmap_size) {
            void *bitmap_addr = (void *)(memmap.address[i].base + MEM_PHYS_OFFSET);


            Bitmap _bitmap(bitmap_addr);
            bitmap = _bitmap;

            memmap.address[i].size -= bitmap_size;
            memmap.address[i].base += bitmap_size;

            // Initialise entire bitmap to 1 (non-free)
            for (uintptr_t j = 0; j < bitmap_size; j++)
                ((uint8_t *)bitmap_addr)[j] = 0xff;

            break;
        }
    }

    // Third, populate free bitmap entries according to memory map.
    for (size_t i = 0; i < memmap.entries; i++) {
        if (memmap.address[i].type != USABLE)
            continue;

        for (uintptr_t j = 0; j < memmap.address[i].size; j += PAGE_SIZE)
            bitmap.unset((memmap.address[i].base + j) / PAGE_SIZE);
    }
}

static void *inner_alloc(size_t count, size_t limit) {
    size_t p = 0;

    while (last_used_index < limit) {
        if (!bitmap.is_set(last_used_index++)) {
            if (++p == count) {
                size_t page = last_used_index - count;
                for (size_t i = page; i < last_used_index; i++) {
                    bitmap.set(i);
                }
                return (void *)(page * PAGE_SIZE);
            }
        } else {
            p = 0;
        }
    }

    return nullptr;
}

void *pmm_alloc(size_t count) {
    size_t l = last_used_index;
    void *ret = inner_alloc(count, highest_page / PAGE_SIZE);
    if (ret == nullptr) {
        last_used_index = 0;
        ret = inner_alloc(count, l);
    }

    return ret;
}

void *pmm_allocz(size_t count) {
    char *ret = (char *)pmm_alloc(count);

    if (ret == nullptr)
        return nullptr;

    uint64_t *ptr = (uint64_t *)(ret + MEM_PHYS_OFFSET);

    for (size_t i = 0; i < count * (PAGE_SIZE / sizeof(uint64_t)); i++)
        ptr[i] = 0;

    return ret;
}

void pmm_free(void *ptr, size_t count) {
    size_t page = (size_t)ptr / PAGE_SIZE;
    for (size_t i = page; i < page + count; i++)
        bitmap.unset(i);
}
