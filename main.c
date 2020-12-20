#include <sys/gdt.h>
#include <sys/idt.h>
//#include <sys/pci.h>
#include <sys/apic.h>
#include <sys/hpet.h>
#include <sys/cpu.h>
#include <mm/pmm.h>
#include <limine/stivale/stivale.h>
#include <lib/dmesg.h>
#include <lib/print.h>
#include <lib/alarm.h>
#include <acpi/acpi.h>

void main(struct stivale_struct *stivale_struct) {
    gdt_init();
    idt_init();
    pmm_init((void *)stivale_struct->memory_map_addr,
             stivale_struct->memory_map_entries);
    dmesg_enable();
    print("Lyre says hello world!\n");

    acpi_init((void *)stivale_struct->rsdp);
    apic_init();
    hpet_init();
    cpu_init();

    alarm_init();

    //pci_init();

    for (;;) {
        asm volatile ("hlt":::"memory");
    }
}
