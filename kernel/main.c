#include <stdint.h>
#include <stddef.h>
#include <sys/gdt.h>
#include <sys/idt.h>
//#include <sys/pci.h>
#include <sys/apic.h>
#include <sys/hpet.h>
#include <sys/cpu.h>
#include <mm/pmm.h>
#include <mm/vmm.h>
#include <stivale/stivale2.h>
#include <lib/dmesg.h>
#include <lib/print.h>
#include <acpi/acpi.h>
#include <fs/vfs.h>
#include <fs/tmpfs.h>

void main(struct stivale2_struct *stivale2_struct) {
    stivale2_struct = (void *)stivale2_struct + MEM_PHYS_OFFSET;

    gdt_init();
    idt_init();

    struct stivale2_struct_tag_memmap *memmap_tag =
        stivale2_get_tag(stivale2_struct, STIVALE2_STRUCT_TAG_MEMMAP_ID);

    pmm_init((void *)memmap_tag->memmap, memmap_tag->entries);
    vmm_init((void *)memmap_tag->memmap, memmap_tag->entries);
    dmesg_enable();
    print("Lyre says hello world!\n");

    struct stivale2_struct_tag_rsdp *rsdp_tag =
        stivale2_get_tag(stivale2_struct, STIVALE2_STRUCT_TAG_RSDP_ID);

    acpi_init((void *)rsdp_tag->rsdp + MEM_PHYS_OFFSET);
    apic_init();
    hpet_init();

    struct stivale2_struct_tag_smp *smp_tag =
        stivale2_get_tag(stivale2_struct, STIVALE2_STRUCT_TAG_SMP_ID);

    smp_init(smp_tag);

    //pci_init();

    vfs_dump_nodes(NULL, "");
    vfs_install_fs(&tmpfs);
    vfs_mount("", "/", "tmpfs");
    struct resource *h = vfs_open("/test.txt", O_RDWR | O_CREAT, 0644);
    if (h == NULL)
        print("a\n");
    vfs_dump_nodes(NULL, "");
    h->write(h, "hello world", 0, 11);

    struct resource *h1 = vfs_open("/test.txt", O_RDWR, 0644);
    if (h1 == NULL)
        print("b\n");
    char buf[20] = {0};
    h1->read(h1, buf, 0, 11);
    print(buf);

    for (;;) {
        asm volatile ("hlt":::"memory");
    }
}
