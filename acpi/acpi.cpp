#include <stdint.h>
#include <stddef.h>
#include <lib/print.hpp>
#include <lib/builtins.h>
#include <acpi/acpi.hpp>
#include <mm/vmm.hpp>

struct RSDP {
    char signature[8];
    uint8_t checksum;
    char oem_id[6];
    uint8_t rev;
    uint32_t rsdt_addr;
    // ver 2.0 only
    uint32_t length;
    uint64_t xsdt_addr;
    uint8_t ext_checksum;
    uint8_t reserved[3];
} __attribute__((packed));

struct RSDT {
    SDT    sdt;
    symbol ptrs_start;
} __attribute__((packed));

static bool use_xsdt;
static RSDT *rsdt;

/* This function should look for all the ACPI tables and index them for
   later use */
void acpi_init(RSDP *rsdp) {
    print("acpi: Revision: %u\n", rsdp->rev);

    if (rsdp->rev >= 2 && rsdp->xsdt_addr) {
        use_xsdt = true;
        print("acpi: Found XSDT at %X\n", ((uintptr_t)rsdp->xsdt_addr + MEM_PHYS_OFFSET));
        rsdt = (RSDT *)((uintptr_t)rsdp->xsdt_addr + MEM_PHYS_OFFSET);
    } else {
        use_xsdt = false;
        print("acpi: Found RSDT at %X\n", ((uintptr_t)rsdp->rsdt_addr + MEM_PHYS_OFFSET));
        rsdt = (RSDT *)((uintptr_t)rsdp->rsdt_addr + MEM_PHYS_OFFSET);
    }
}

/* Find SDT by signature */
void *acpi_find_sdt(const char *signature, int index) {
    int cnt = 0;
    size_t interval = use_xsdt ? 8 : 4;

    for (size_t i = 0; i < rsdt->sdt.length - sizeof(SDT); i += interval) {
        SDT *ptr = (SDT *)((uintptr_t)rsdt->ptrs_start + i + MEM_PHYS_OFFSET);
        if (!strncmp(ptr->signature, signature, 4) && cnt++ == index) {
            print("acpi: Found \"%s\" at %X\n", signature, ptr);
            return (void *)ptr;
        }
    }

    print("acpi: \"%s\" not found\n", signature);
    return nullptr;
}
