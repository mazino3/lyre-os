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
#include <fs/devtmpfs.h>
#include <sched/sched.h>
#include <misc/initramfs.h>

__attribute__((noreturn))
static void main_thread(struct stivale2_struct *stivale2_struct) {
    vfs_dump_nodes(NULL, "");
    vfs_install_fs(&tmpfs);
    vfs_install_fs(&devtmpfs);
    vfs_mount("tmpfs", "/", "tmpfs");
    vfs_dump_nodes(NULL, "");
    vfs_mkdir(NULL, "/dev", 0755, true);
    vfs_dump_nodes(NULL, "");
    vfs_mount("devtmpfs", "/dev", "devtmpfs");

    dev_init();
    vfs_dump_nodes(NULL, "");

    struct stivale2_struct_tag_modules *modules_tag =
        stivale2_get_tag(stivale2_struct, STIVALE2_STRUCT_TAG_MODULES_ID);

    initramfs_init(modules_tag);

    struct stivale2_struct_tag_memmap *memmap_tag =
        stivale2_get_tag(stivale2_struct, STIVALE2_STRUCT_TAG_MEMMAP_ID);

    pmm_reclaim_memory((void *)memmap_tag->memmap, memmap_tag->entries);

    print("CPU %u\n", this_cpu->cpu_number);

    const char *argv[] = { "/usr/sbin/udevd", "--debug", NULL };
    const char *envp[] = { NULL };
    sched_start_program("/usr/sbin/udevd", argv, envp,
                        "/dev/e9", "/dev/e9", "/dev/e9");

    for (;;) asm ("hlt");
}

__attribute__((noreturn))
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

    sched_new_thread(NULL, false, main_thread, stivale2_struct, NULL, NULL, NULL);

    asm ("sti");
    for (;;) asm ("hlt");
}
