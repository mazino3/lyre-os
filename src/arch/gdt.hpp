#pragma once

#include <stddef.h>

void gdt_init();
void gdt_load_tss(size_t addr);
