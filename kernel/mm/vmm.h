#ifndef __MM__VMM_H__
#define __MM__VMM_H__

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <lib/types.h>
#include <lib/resource.h>
#include <lib/dynarray.h>
#include <stivale/stivale2.h>

#define PAGE_SIZE ((size_t)4096)
#define MEM_PHYS_OFFSET ((uintptr_t)0xffff800000000000)
#define KERNEL_BASE ((uintptr_t)0xffffffff80000000)

#define MMAP_ANON_NON_FIXED_BASE ((uintptr_t)0x71000000000)

#define PTE_PRESENT  (1 << 0)
#define PTE_WRITABLE (1 << 1)
#define PTE_USER     (1 << 2)

#define PROT_NONE  0x00
#define PROT_READ  0x01
#define PROT_WRITE 0x02
#define PROT_EXEC  0x04

#define MAP_PRIVATE   0x01
#define MAP_SHARED    0x02
#define MAP_FIXED     0x04
#define MAP_ANON      0x08
#define MAP_ANONYMOUS 0x08

#define MAP_FAILED ((void *)-1)

struct pagemap;

struct mmap_range_global {
    DYNARRAY_STRUCT(struct pagemap *) pagemaps;
    struct resource *res;
    uintptr_t base;
    size_t    length;
    off_t     offset;
};

struct mmap_range_local {
    struct mmap_range_global *global;
    uintptr_t base;
    size_t    length;
    off_t     offset;
    int       prot;
    int       flags;
};

extern struct pagemap *kernel_pagemap;

void vmm_init(struct stivale2_mmap_entry *memmap, size_t memmap_entries);
void vmm_switch_pagemap(struct pagemap *pagemap);
struct pagemap *vmm_new_pagemap(void);
struct pagemap *vmm_fork_pagemap(struct pagemap *old);
bool vmm_erase_pagemap(struct pagemap *pagemap);
bool vmm_map_page(struct pagemap *pagemap, uintptr_t virt_addr, uintptr_t phys_addr,
                  uintptr_t flags);

bool mmap_range(struct pagemap *pagemap, uintptr_t virt_addr, uintptr_t phys_addr,
                   size_t length, int prot, int flags);
void *mmap(struct pagemap *pm, void *addr, size_t length, int prot, int flags,
           struct resource *res, off_t offset);

void vmm_page_fault_handler(void);

#endif
