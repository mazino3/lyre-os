#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <mm/vmm.h>
#include <mm/pmm.h>
#include <lib/lock.h>
#include <lib/dynarray.h>
#include <lib/print.h>
#include <lib/errno.h>
#include <stivale/stivale2.h>
#include <sys/cpu.h>
#include <sched/sched.h>

static enum {
    VMM_4LEVEL_PG,
    VMM_5LEVEL_PG
} vmm_paging_type;

struct pagemap *kernel_pagemap = NULL;

struct event *page_fault_event;

void vmm_init(struct stivale2_mmap_entry *memmap, size_t memmap_entries) {
    vmm_paging_type = VMM_4LEVEL_PG;

    kernel_pagemap = vmm_new_pagemap();

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

    page_fault_event = event_create(1);
}

void vmm_switch_pagemap(struct pagemap *pagemap) {
    asm volatile (
        "mov cr3, %0"
        :
        : "r" (pagemap->top_level)
        : "memory"
    );
}

static inline uintptr_t entries_to_virt_addr(size_t pml5_entry,
                                             size_t pml4_entry,
                                             size_t pdpt_entry,
                                             size_t pd_entry,
                                             size_t pt_entry) {
    uintptr_t virt_addr = 0;

    virt_addr |= pml5_entry << 48;
    virt_addr |= pml4_entry << 39;
    virt_addr |= pdpt_entry << 30;
    virt_addr |= pd_entry << 21;
    virt_addr |= pt_entry << 12;

    return virt_addr;
}

static uintptr_t *virt2pte(struct pagemap *pagemap,
                           uintptr_t virt_addr, bool allocate);

struct pagemap *vmm_fork_pagemap(struct pagemap *old) {
    struct pagemap *new = vmm_new_pagemap();

    SPINLOCK_ACQUIRE(old->lock);

    for (size_t i = 0; i < old->mmap_ranges.length; i++) {
        struct mmap_range_local *r = old->mmap_ranges.storage[i];
        struct mmap_range_global *g = r->global;

        struct mmap_range_local *new_range = alloc(sizeof(struct mmap_range_local));
        *new_range = *r;

        if (g->res != NULL) {
            LOCKED_INC(g->res->refcount);
        }

        if (r->flags & MAP_SHARED) {
            new_range->global = g;

            DYNARRAY_PUSHBACK(g->locals, new_range);

            for (size_t i = r->base; i < r->base + r->length; i += PAGE_SIZE) {
                uintptr_t *pte_old = virt2pte(old, i, false);
                if (pte_old == NULL)
                    continue;
                uintptr_t *pte_new = virt2pte(new, i, true);
                if (pte_new == NULL)
                    continue;
                *pte_new = *pte_old;
            }
        } else {
            new_range->global = alloc(sizeof(struct mmap_range_global));
            *new_range->global = *g;

            new_range->global->locals = (typeof(new_range->global->locals)){0};

            new_range->global->shadow_pagemap = (struct pagemap){0};
            new_range->global->shadow_pagemap.top_level = pmm_allocz(1);

            DYNARRAY_PUSHBACK(new_range->global->locals, new_range);

            if (r->flags & MAP_ANONYMOUS) {
                for (size_t i = r->base; i < r->base + r->length; i += PAGE_SIZE) {
                    uintptr_t *pte_old = virt2pte(old, i, false);
                    if (pte_old == NULL)
                        continue;
                    uintptr_t *pte_new = virt2pte(new, i, true);
                    if (pte_new == NULL)
                        continue;
                    uintptr_t *spte_new = virt2pte(&new_range->global->shadow_pagemap, i, true);
                    if (spte_new == NULL)
                        continue;
                    void *page = pmm_allocz(1);
                    memcpy(page + MEM_PHYS_OFFSET,
                           (void*)(*pte_old & ~((uintptr_t)0xfff)) + MEM_PHYS_OFFSET,
                           PAGE_SIZE);
                    *pte_new = (*pte_old & ((uintptr_t)0xfff)) | (uintptr_t)page;
                    *spte_new = *pte_new;
                }
            } else {
                g->res->mmap(g->res, new_range);
            }
        }
    }

    LOCK_RELEASE(old->lock);

    return new;
}

bool vmm_erase_pagemap(struct pagemap *pagemap) {
    SPINLOCK_ACQUIRE(pagemap->lock);

    for (size_t i = 0; i < pagemap->mmap_ranges.length; i++) {
        struct mmap_range_local *r = pagemap->mmap_ranges.storage[i];

        munmap(pagemap, (void*)r->base, r->length);
    }

    free(pagemap);
    return true;
}

struct pagemap *vmm_new_pagemap(void) {
    struct pagemap *pagemap = alloc(sizeof(struct pagemap));
    pagemap->top_level   = pmm_allocz(1);
    if (kernel_pagemap != NULL) {
        uintptr_t *top_level =
            (void*)pagemap->top_level + MEM_PHYS_OFFSET;
        uintptr_t *kernel_top_level =
            (void*)kernel_pagemap->top_level + MEM_PHYS_OFFSET;
        for (size_t i = 256; i < 512; i++)
            top_level[i] = kernel_top_level[i];
    }
    return pagemap;
}

static uintptr_t *get_next_level(uintptr_t *current_level, size_t entry,
                                 bool allocate) {
    uintptr_t ret;

    if (current_level[entry] & 0x1) {
        // Present flag set
        ret = current_level[entry] & ~((uintptr_t)0xfff);
    } else {
        if (!allocate)
            return NULL;

        // Allocate a table for the next level
        ret = (uintptr_t)pmm_allocz(1);
        if (ret == 0) {
            return NULL;
        }
        // Present + writable + user (0b111)
        current_level[entry] = ret | 0b111;
    }

    return (void *)ret + MEM_PHYS_OFFSET;
}

static uintptr_t *virt2pte(struct pagemap *pagemap,
                           uintptr_t virt_addr, bool allocate) {
    // Calculate the indices in the various tables using the virtual address
    uintptr_t pml5_entry = (virt_addr & ((uintptr_t)0x1ff << 48)) >> 48;
    uintptr_t pml4_entry = (virt_addr & ((uintptr_t)0x1ff << 39)) >> 39;
    uintptr_t pml3_entry = (virt_addr & ((uintptr_t)0x1ff << 30)) >> 30;
    uintptr_t pml2_entry = (virt_addr & ((uintptr_t)0x1ff << 21)) >> 21;
    uintptr_t pml1_entry = (virt_addr & ((uintptr_t)0x1ff << 12)) >> 12;

    uintptr_t *pml5, *pml4, *pml3, *pml2, *pml1;

    // Paging levels
    switch (vmm_paging_type) {
        case VMM_5LEVEL_PG:
            pml5 = (void*)pagemap->top_level + MEM_PHYS_OFFSET;
            goto level5;
        case VMM_4LEVEL_PG:
            pml4 = (void*)pagemap->top_level + MEM_PHYS_OFFSET;
            goto level4;
        default:
            for (;;);
    }

level5:
    pml4 = get_next_level(pml5, pml5_entry, allocate);
    if (pml4 == NULL)
        return NULL;
level4:
    pml3 = get_next_level(pml4, pml4_entry, allocate);
    if (pml3 == NULL)
        return NULL;
    pml2 = get_next_level(pml3, pml3_entry, allocate);
    if (pml2 == NULL)
        return NULL;
    pml1 = get_next_level(pml2, pml2_entry, allocate);
    if (pml1 == NULL)
        return NULL;

    return &pml1[pml1_entry];
}

#define INVALID_PHYS ((uintptr_t)0xffffffffffffffff)

uintptr_t virt2phys(struct pagemap *pagemap, uintptr_t virt_addr) {
    if (!pagemap) {
        pagemap = this_cpu->current_thread->process->pagemap;
    }

    uintptr_t *pte_p = virt2pte(pagemap, virt_addr, false);
    if (pte_p == NULL || !(*pte_p & PTE_PRESENT))
        return INVALID_PHYS;

    return *pte_p & ~((uintptr_t)0xfff);
}

static void invalidate_tlb(struct pagemap *pagemap, uintptr_t virt_addr) {
    uintptr_t cr3 = read_cr("3");
    if ((cr3 & ~((uintptr_t)0xfff))
     == ((uintptr_t)pagemap->top_level & ~((uintptr_t)0xfff))) {
        invlpg((void*)virt_addr);
    }
}

static bool unmap_page(struct pagemap *pagemap, uintptr_t virt_addr) {
    uintptr_t *pte_p = virt2pte(pagemap, virt_addr, false);
    if (pte_p == NULL) {
        return false;
    }

    *pte_p = 0;

    invalidate_tlb(pagemap, virt_addr);

    return true;
}

bool vmm_map_page(struct pagemap *pagemap, uintptr_t virt_addr, uintptr_t phys_addr,
                  uintptr_t flags) {
    SPINLOCK_ACQUIRE(pagemap->lock);

    uintptr_t *pte_p = virt2pte(pagemap, virt_addr, true);
    if (pte_p == NULL) {
        LOCK_RELEASE(pagemap->lock);
        return false;
    }

    *pte_p = phys_addr | flags;

    invalidate_tlb(pagemap, virt_addr);

    LOCK_RELEASE(pagemap->lock);

    return true;
}

struct addr2range_hit {
    bool failed;
    struct mmap_range_local *range;
    size_t memory_page;
    size_t file_page;
};

static struct addr2range_hit addr2range(struct pagemap *pm, uintptr_t addr) {
    for (size_t i = 0; i < pm->mmap_ranges.length; i++) {
        struct mmap_range_local *r = pm->mmap_ranges.storage[i];
        if (addr >= r->base && addr < r->base + r->length) {
            struct addr2range_hit hit = {0};

            hit.failed      = false;
            hit.range       = r;
            hit.memory_page = addr / PAGE_SIZE;
            hit.file_page   = r->offset / PAGE_SIZE +
                              (addr / PAGE_SIZE - r->base / PAGE_SIZE);

            return hit;
        }
    }
    return (struct addr2range_hit){ .failed = true };
}

bool mmap_map_page_in_range(struct mmap_range_global *g, uintptr_t virt_addr,
                            uintptr_t phys_addr, int prot) {
    vmm_map_page(&g->shadow_pagemap, virt_addr, phys_addr,
                 PTE_PRESENT | PTE_USER |
                 (prot & PROT_WRITE ? PTE_WRITABLE : 0));

    for (size_t i = 0; i < g->locals.length; i++) {
        struct mmap_range_local *l = g->locals.storage[i];
        if (virt_addr < l->base || virt_addr >= l->base + l->length)
            continue;
        vmm_map_page(g->locals.storage[i]->pagemap, virt_addr, phys_addr,
                     PTE_PRESENT | PTE_USER |
                     (prot & PROT_WRITE ? PTE_WRITABLE : 0));
    }

    return true;
}

void _vmm_page_fault_handler(struct cpu_gpr_context *ctx, uintptr_t addr) {
    if (ctx->cs & 0x03) {
        swapgs();
    }

    struct pagemap *pagemap = this_cpu->current_thread->process->pagemap;

    SPINLOCK_ACQUIRE(pagemap->lock);

    struct addr2range_hit hit = addr2range(pagemap, addr);

    LOCK_RELEASE(pagemap->lock);

    if (hit.failed) {
        print("PANIC unhandled page fault at %X\n", ctx->rip);
        for (;;);
    }

    asm ("sti");

    if (hit.range->flags & MAP_ANONYMOUS) {
        void *page = pmm_allocz(1);
        mmap_map_page_in_range(hit.range->global, hit.memory_page * PAGE_SIZE,
                               (uintptr_t)page, hit.range->prot);
    } else {
        hit.range->global->res->mmap_hit(hit.range->global->res, hit.range,
                                         hit.memory_page,
                                         hit.file_page);
    }

    asm ("cli");

    if (ctx->cs & 0x03) {
        swapgs();
    }
}

bool mmap_range(struct pagemap *pm, uintptr_t virt_addr, uintptr_t phys_addr,
                size_t length, int prot, int flags) {
    flags |= MAP_ANONYMOUS;

    void *pool = alloc(sizeof(struct mmap_range_local)
                     + sizeof(struct mmap_range_global));
    struct mmap_range_local  *range_local  = pool;
    struct mmap_range_global *range_global = pool + sizeof(struct mmap_range_local);

    range_local->pagemap = pm;
    range_local->global  = range_global;
    range_local->base    = virt_addr;
    range_local->length  = length;
    range_local->prot    = prot;
    range_local->flags   = flags;

    DYNARRAY_INSERT(range_global->locals, range_local);
    range_global->base   = virt_addr;
    range_global->length = length;

    range_global->shadow_pagemap.top_level = pmm_allocz(1);

    SPINLOCK_ACQUIRE(pm->lock);
    DYNARRAY_INSERT(pm->mmap_ranges, range_local);
    LOCK_RELEASE(pm->lock);

    for (size_t i = 0; i < length; i += PAGE_SIZE) {
        mmap_map_page_in_range(range_global, virt_addr + i, phys_addr + i, prot);
    }

    return true;
}

bool munmap(struct pagemap *pm, void *addr, size_t length) {
    if (length % PAGE_SIZE || length == 0) {
        print("munmap: length is not a multiple of PAGE_SIZE or is 0\n");
        errno = EINVAL;
        return false;
    }

    for (uintptr_t i = (uintptr_t)addr;
      i < (uintptr_t)addr + length;
      i += PAGE_SIZE) {
        struct addr2range_hit hit = addr2range(pm, i);
        if (hit.failed)
            continue;

        struct mmap_range_local *r = hit.range;
        struct mmap_range_global *g = r->global;

        uintptr_t begin_snip = i;
        for (;;) {
            i += PAGE_SIZE;
            if (i >= r->base + r->length || i >= (uintptr_t)addr + length)
                break;
        }
        uintptr_t end_snip = i;
        uintptr_t snip_size = end_snip - begin_snip;

        if (begin_snip > r->base && end_snip < r->base + r->length) {
            // We will have to split the range in 2
            print("munmap: range splits not yet supported\n");
            errno = EINVAL;
            return false;
        }

        for (uintptr_t i = begin_snip; i < end_snip; i += PAGE_SIZE)
            unmap_page(pm, i);

        if (snip_size == r->length) {
            DYNARRAY_REMOVE_BY_VALUE_AND_PACK(pm->mmap_ranges, r);
        }

        if (snip_size == r->length && g->locals.length == 1) {
            if (r->flags & MAP_ANONYMOUS) {
                for (uintptr_t i = g->base;
                  i < g->base + g->length; i += PAGE_SIZE) {
                    uintptr_t p = virt2phys(&g->shadow_pagemap, i);
                    if (p == INVALID_PHYS)
                        continue;
                    unmap_page(&g->shadow_pagemap, i);
                    pmm_free((void*)p, 1);
                }
            } else {
                g->res->munmap(g->res, r);
            }
        } else {
            if (begin_snip == r->base)
                r->base = end_snip;
            else
                r->length -= end_snip - begin_snip;
        }
    }

    return true;
}

void *mmap(struct pagemap *pm, void *addr, size_t length, int prot, int flags,
           struct resource *res, off_t offset) {/*
    print("mmap(pm: %X, addr: %X, len: %X,\n"
          "     prot:  %s%s%s%s,\n"
          "     flags: %s%s%s%s);\n",
          pm, addr, length,
          prot & PROT_READ  ? "PROT_READ ":"",
          prot & PROT_WRITE ? "PROT_WRITE ":"",
          prot & PROT_EXEC  ? "PROT_EXEC ":"",
          prot & PROT_NONE  ? "PROT_NONE ":"",
          flags & MAP_SHARED    ? "MAP_SHARED ":"",
          flags & MAP_PRIVATE   ? "MAP_PRIVATE ":"",
          flags & MAP_FIXED     ? "MAP_FIXED ":"",
          flags & MAP_ANONYMOUS ? "MAP_ANONYMOUS ":"");*/

    if (length % PAGE_SIZE || length == 0) {
        print("mmap: length is not a multiple of PAGE_SIZE or is 0\n");
        errno = EINVAL;
        return MAP_FAILED;
    }

    struct process *process = this_cpu->current_thread->process;

    uintptr_t base;
    if (flags & MAP_FIXED) {
        base = (uintptr_t)addr;
    } else {
        base = process->mmap_anon_non_fixed_base;
        process->mmap_anon_non_fixed_base += length + PAGE_SIZE;
    }

    void *pool = alloc(sizeof(struct mmap_range_local)
                     + sizeof(struct mmap_range_global));
    struct mmap_range_local  *range_local  = pool;
    struct mmap_range_global *range_global = pool + sizeof(struct mmap_range_local);

    range_local->pagemap = pm;
    range_local->global  = range_global;
    range_local->base    = base;
    range_local->length  = length;
    range_local->offset  = offset;
    range_local->prot    = prot;
    range_local->flags   = flags;

    DYNARRAY_INSERT(range_global->locals, range_local);
    range_global->base   = base;
    range_global->length = length;
    range_global->res    = res;
    range_global->offset = offset;

    range_global->shadow_pagemap.top_level = pmm_allocz(1);

    SPINLOCK_ACQUIRE(pm->lock);
    DYNARRAY_INSERT(pm->mmap_ranges, range_local);
    LOCK_RELEASE(pm->lock);

    if (res != NULL)
        res->refcount++;

    return (void *)base;
}

void syscall_mmap(struct cpu_gpr_context *ctx) {
    void  *addr   = (void *) ctx->rdi;
    size_t length = (size_t) ctx->rsi;
    int    prot   = (int)    ctx->rdx;
    int    flags  = (int)    ctx->r10;
    int    fd     = (int)    ctx->r8;
    off_t  offset = (off_t)  ctx->r9;

    struct resource *res = NULL;

    if (!(flags & MAP_ANONYMOUS)) {
        res = resource_from_fd(fd);
        if (res == NULL) {
            ctx->rax = (uint64_t)-1;
            return;
        }
    }

    ctx->rax = (uint64_t)mmap(this_cpu->current_thread->process->pagemap,
                              addr, length, prot, flags, res, offset);
}

void syscall_munmap(struct cpu_gpr_context *ctx) {
    void   *addr  = (void *) ctx->rdi;
    size_t length = (size_t) ctx->rsi;

    bool ret = munmap(this_cpu->current_thread->process->pagemap, addr, length);

    if (ret == false)
        ctx->rax = (uint64_t)-1;
    else
        ctx->rax = (uint64_t)0;
}
