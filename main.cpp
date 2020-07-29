#include <sys/gdt.hpp>
#include <sys/idt.hpp>
#include <sys/pci.hpp>
#include <mm/pmm.hpp>
#include <lib/stivale.hpp>
#include <lib/dmesg.hpp>
#include <lib/print.hpp>
#include <acpi/acpi.hpp>

extern "C" void main(Stivale *sti) {
    gdt_init();
    idt_init();
    pmm_init(sti->memmap);
    dmesg_enable();
    print("Lyre says hello world!\n");

    acpi_init((RSDP *)sti->rsdp);
    pci_init();

    for (;;) {
        asm volatile ("hlt":::"memory");
    }
}
