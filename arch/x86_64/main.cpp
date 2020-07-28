#include <gdt.hpp>
#include <interrupts/idt.hpp>
#include <mm/pmm.hpp>
#include <stivale.hpp>
#include <init.hpp>
#include <mm/memmap.hpp>
#include <lib/dmesg.hpp>

extern "C" void main(Stivale *sti) {
    gdt_init();
    idt_init();
    memmap_init(sti);
    pmm_init(sti->memmap);
    dmesg_enable();

    kernel_main();
}
