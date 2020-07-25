#include <gdt.hpp>
#include <mm/pmm.hpp>
#include <stivale.hpp>
#include <init.hpp>

extern "C" void main(Stivale *sti) {
    gdt_init();
    pmm_init(sti->memmap);

    kernel_main();
}
