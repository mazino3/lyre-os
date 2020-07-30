#include <stdint.h>
#include <stddef.h>
#include <acpi/acpi.hpp>
#include <acpi/madt.hpp>
#include <lib/dynarray.hpp>
#include <lib/print.hpp>

MADT *madt;

DynArray<MADTLocalApic *> madt_local_apics;
DynArray<MADTIOApic *>    madt_io_apics;
DynArray<MADTISO *>       madt_isos;
DynArray<MADTNMI *>       madt_nmis;

void init_madt() {
    // search for MADT table
    madt = (MADT *)acpi_find_sdt("APIC", 0);

    // parse the MADT entries
    for (uint8_t *madt_ptr = (uint8_t *)madt->madt_entries_begin;
        (uintptr_t)madt_ptr < (uintptr_t)madt + madt->sdt.length;
        madt_ptr += *(madt_ptr + 1)) {
        switch (*(madt_ptr)) {
            case 0:
                // processor local APIC
                print("acpi/madt: Found local APIC #%U\n", madt_local_apics.length());
                madt_local_apics.push_back((MADTLocalApic *)madt_ptr);
                break;
            case 1:
                // I/O APIC
                print("acpi/madt: Found I/O APIC #%U\n", madt_io_apics.length());
                madt_io_apics.push_back((MADTIOApic *)madt_ptr);
                break;
            case 2:
                // interrupt source override
                print("acpi/madt: Found ISO #%U\n", madt_isos.length());
                madt_isos.push_back((MADTISO *)madt_ptr);
                break;
            case 4:
                // NMI
                print("acpi/madt: Found NMI #%U\n", madt_nmis.length());
                madt_nmis.push_back((MADTNMI *)madt_ptr);
                break;
            default:
                break;
        }
    }
}
