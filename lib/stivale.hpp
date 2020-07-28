#pragma once

#include <stdint.h>

struct StivaleModule {
    uint64_t begin;
    uint64_t end;
    char     name[128];
} __attribute__((packed));

enum StivaleMemmapEntryType : uint32_t {
    STIVALE_USABLE      = 1,
    STIVALE_RESERVED    = 2,
    STIVALE_ACPIRECLAIM = 3,
    STIVALE_ACPINVS     = 4,
    STIVALE_KERNEL      = 10
};

struct StivaleMemmapEntry {
    uint64_t base;
    uint64_t size;
    StivaleMemmapEntryType type;
    uint32_t unused;
} __attribute__((packed));

struct StivaleMemmap {
    StivaleMemmapEntry *address;
    uint64_t            entries;
} __attribute__((packed));

struct StivaleFramebuffer {
    uint64_t address;
    uint16_t pitch;
    uint16_t width;
    uint16_t height;
    uint16_t bpp;
} __attribute__((packed));

struct Stivale {
    uint64_t           cmdline;
    StivaleMemmap      memmap;
    StivaleFramebuffer framebuffer;
    uint64_t           rsdp;
    uint64_t           module_count;
    StivaleModule     *modules;
    uint64_t           epoch;
    uint64_t           flags;
} __attribute__((packed));
