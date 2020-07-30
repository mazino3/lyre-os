#pragma once

#include <stdint.h>
#include <stddef.h>
#include <lib/builtins.h>
#include <acpi/acpi.hpp>
#include <lib/dynarray.hpp>

struct MADT {
    SDT      sdt;
    uint32_t local_controller_addr;
    uint32_t flags;
    symbol   madt_entries_begin;
} __attribute__((packed));

struct MADTHeader {
    uint8_t type;
    uint8_t length;
} __attribute__((packed));

struct MADTLocalApic {
    MADTHeader header;
    uint8_t    processor_id;
    uint8_t    apic_id;
    uint32_t   flags;
} __attribute__((packed));

struct MADTIOApic {
    MADTHeader header;
    uint8_t    apic_id;
    uint8_t    reserved;
    uint32_t   addr;
    uint32_t   gsib;
} __attribute__((packed));

struct MADTISO {
    MADTHeader header;
    uint8_t    bus_source;
    uint8_t    irq_source;
    uint32_t   gsi;
    uint16_t   flags;
} __attribute__((packed));

struct MADTNMI {
    MADTHeader header;
    uint8_t    processor;
    uint16_t   flags;
    uint8_t    lint;
} __attribute__((packed));

extern MADT *madt;

extern DynArray<MADTLocalApic *> madt_local_apics;
extern DynArray<MADTIOApic *>    madt_io_apics;
extern DynArray<MADTISO *>       madt_isos;
extern DynArray<MADTNMI *>       madt_nmis;

void init_madt();
