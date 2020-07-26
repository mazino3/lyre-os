#include <gdt.hpp>
#include <mm/pmm.hpp>
#include <stivale.hpp>
#include <init.hpp>
#include <mm/memmap.hpp>

extern "C" void main(Stivale *sti) {
    gdt_init();
    memmap_init(sti);
    pmm_init(sti->memmap);
    
    kernel_main();
}
