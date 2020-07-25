#include <gdt.hpp>
#include <mm/pmm.hpp>
#include <stivale.hpp>

extern "C" void main(Stivale *sti) {
    gdt_init();
    pmm_init(sti->memmap);

    for (;;);
}
