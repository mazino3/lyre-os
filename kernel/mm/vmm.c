#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <mm/vmm.h>
#include <mm/pmm.h>
#include <lib/lock.h>
#include <lib/dynarray.h>
#include <stivale/stivale.h>

struct mmap_range {
    uintptr_t base;
    size_t    length;
    struct resource *res;
    off_t     offset;
    int       prot;
    int       flags;
};

struct pagemap {
    lock_t lock;
    enum paging_type paging_type;
    uintptr_t *top_level;
    //DYNARRAY_NEW(struct mmap_range *, mmap_ranges);
};

static struct pagemap *kernel_pagemap;

void vmm_init(struct stivale_mmap_entry *memmap, size_t memmap_entries) {
    kernel_pagemap = vmm_new_pagemap(PAGING_4LV);

    for (uintptr_t p = 0; p < 0x100000000; p += PAGE_SIZE) {
        vmm_map_page(kernel_pagemap, MEM_PHYS_OFFSET + p, p, 0x03);
    }

    for (uintptr_t p = 0; p < 0x80000000; p += PAGE_SIZE) {
        vmm_map_page(kernel_pagemap, KERNEL_BASE + p, p, 0x03);
    }

    for (size_t i = 0; i < memmap_entries; i++) {
        for (uintptr_t p = 0; p < memmap[i].length; p += PAGE_SIZE)
            vmm_map_page(kernel_pagemap, MEM_PHYS_OFFSET + p, p, 0x03);
    }

    vmm_switch_pagemap(kernel_pagemap);
}

void vmm_switch_pagemap(struct pagemap *pagemap) {
    asm volatile (
        "mov cr3, %0"
        :
        : "r" (pagemap->top_level)
        : "memory"
    );
}

struct pagemap *vmm_new_pagemap(enum paging_type paging_type) {
    struct pagemap *pagemap = alloc(sizeof(struct pagemap));
    pagemap->paging_type = paging_type;
    pagemap->top_level   = pmm_allocz(1);
    return pagemap;
}

static uintptr_t *get_next_level(uintptr_t *current_level, size_t entry) {
    uintptr_t ret;

    if (current_level[entry] & 0x1) {
        // Present flag set
        ret = current_level[entry] & ~((uintptr_t)0xfff);
    } else {
        // Allocate a table for the next level
        ret = (uintptr_t)pmm_allocz(1);
        if (ret == 0) {
            return NULL;
        }
        // Present + writable + user (0b111)
        current_level[entry] = ret | 0b111;
    }

    return (uintptr_t *)ret;
}

bool vmm_map_page(struct pagemap *pagemap, uintptr_t virt_addr, uintptr_t phys_addr,
                  uintptr_t flags) {
    SPINLOCK_ACQUIRE(pagemap->lock);

    // Calculate the indices in the various tables using the virtual address
    uintptr_t pml5_entry = (virt_addr & ((uintptr_t)0x1ff << 48)) >> 48;
    uintptr_t pml4_entry = (virt_addr & ((uintptr_t)0x1ff << 39)) >> 39;
    uintptr_t pml3_entry = (virt_addr & ((uintptr_t)0x1ff << 30)) >> 30;
    uintptr_t pml2_entry = (virt_addr & ((uintptr_t)0x1ff << 21)) >> 21;
    uintptr_t pml1_entry = (virt_addr & ((uintptr_t)0x1ff << 12)) >> 12;

    uintptr_t *pml5, *pml4, *pml3, *pml2, *pml1;

    // Paging levels
    switch (pagemap->paging_type) {
        case PAGING_5LV:
            pml5 = pagemap->top_level;
            goto level5;
        case PAGING_4LV:
            pml4 = pagemap->top_level;
            goto level4;
        default:
            for (;;);
    }

level5:
    pml4 = get_next_level(pml5, pml5_entry);
    if (pml4 == NULL)
        return false;
level4:
    pml3 = get_next_level(pml4, pml4_entry);
    if (pml3 == NULL)
        return false;
    pml2 = get_next_level(pml3, pml3_entry);
    if (pml2 == NULL)
        return false;
    pml1 = get_next_level(pml2, pml2_entry);
    if (pml1 == NULL)
        return false;

    pml1[pml1_entry] = phys_addr | flags;

    LOCK_RELEASE(pagemap->lock);

    return true;
}

struct addr2range_hit {
    struct mmap_range *range;
    size_t memory_page;
    size_t file_page;
};
/*
static struct addr2range_hit addr2range(struct pagemap *pm, uintptr_t addr) {
    for (size_t i = 0; i < pm->mmap_ranges.length; i++) {
        struct mmap_range *r = pm->mmap_ranges.storage[i];
        if (addr >= r->base && addr < r->base + r->length) {
            struct addr2range_hit hit;

            hit.range       = r;
            hit.memory_page = addr / PAGE_SIZE;
            hit.file_page   = r->offset / PAGE_SIZE +
                              (addr / PAGE_SIZE - r->base / PAGE_SIZE);

            return hit;
        }
    }
    return NULL;
}
/*
void *mmap(struct pagemap *pm, void *addr, size_t length, int prot, int flags,
           struct resource *res, off_t offset) {
    print("mmap(pm: %X, addr: %X, len: %X,\n"
          "     prot:  %s%s%s%s,\n"
          "     flags: %s%s%s%s);\n"
          pm, addr, length,
          prot & PROT_READ  ? "PROT_READ ":"",
          prot & PROT_WRITE ? "PROT_WRITE ":"",
          prot & PROT_EXEC  ? "PROT_EXEC ":"",
          prot & PROT_NONE  ? "PROT_NONE ":"",
          flags & MAP_SHARED    ? "MAP_SHARED ":"",
          flags & MAP_PRIVATE   ? "MAP_PRIVATE ":"",
          flags & MAP_FIXED     ? "MAP_FIXED ":"",
          flags & MAP_ANONYMOUS ? "MAP_ANONYMOUS ":"");

    if ((uintptr_t)addr & PAGE_SIZE - 1) {
        if (flags & MAP_FIXED) {
            // errno = EINVAL;
            return MAP_FAILED;
        }

        addr += PAGE_SIZE;
        addr &= ~(PAGE_SIZE - 1);
    }

    struct addr2range_hit hit = addr2range(pm, );


}
*/
