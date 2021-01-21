#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <lib/elf.h>
#include <lib/math.h>
#include <lib/builtins.h>
#include <lib/alloc.h>
#include <mm/vmm.h>
#include <mm/pmm.h>

bool elf_load(struct pagemap *pagemap, struct resource *file, uintptr_t base,
              struct auxval_t *auxval, char **ld_path) {
    bool ok = false;
    struct elf_phdr_t *phdr = NULL;
    if (ld_path != NULL)
        *ld_path = NULL;
    ssize_t ret;

    struct elf_hdr_t hdr;

    ret = file->read(file, &hdr, 0, sizeof(struct elf_hdr_t));
    if (ret != sizeof(struct elf_hdr_t))
        goto out;

    if (memcmp(hdr.ident, "\177ELF", 4) != 0)
        goto out;

    if (hdr.ident[EI_CLASS] != 0x02
     || hdr.ident[EI_DATA] != BITS_LE
     || hdr.ident[EI_OSABI] != ABI_SYSV
     || hdr.machine != ARCH_X86_64)
        goto out;

    phdr = alloc(hdr.ph_num * sizeof(struct elf_phdr_t));
    if (!phdr)
        goto out;

    ret = file->read(file, phdr, hdr.phoff, hdr.ph_num * sizeof(struct elf_phdr_t));
    if (ret != hdr.ph_num * sizeof(struct elf_phdr_t))
        goto out;

    auxval->at_phdr  = 0;
    auxval->at_phent = sizeof(struct elf_phdr_t);
    auxval->at_phnum = hdr.ph_num;

    for (size_t i = 0; i < hdr.ph_num; i++) {
        switch (phdr[i].p_type) {
            case PT_INTERP: {
                if (ld_path == NULL)
                    break;

                *ld_path = alloc(phdr[i].p_filesz + 1);
                if (*ld_path == NULL)
                    goto out;

                ret = file->read(file, *ld_path, phdr[i].p_offset, phdr[i].p_filesz);
                if (ret != phdr[i].p_filesz)
                    goto out;

                (*ld_path)[phdr[i].p_filesz] = 0;

                break;
            }
            case PT_PHDR: {
                auxval->at_phdr = base + phdr[i].p_vaddr;
                break;
            }
            default:
                if (phdr[i].p_type != PT_LOAD)
                    continue;
        }

        size_t misalign   = phdr[i].p_vaddr & (PAGE_SIZE - 1);
        size_t page_count = DIV_ROUNDUP(misalign + phdr[i].p_memsz, PAGE_SIZE);

        /* Allocate space */
        void *addr = pmm_allocz(page_count);
        if (!addr)
            goto out;

        size_t pf = 0x05;
        if (phdr[i].p_flags & PF_W)
            pf |= 0x02;

        for (size_t j = 0; j < page_count; j++) {
            size_t virt = base + phdr[i].p_vaddr + (j * PAGE_SIZE);
            size_t phys = (size_t)addr + (j * PAGE_SIZE);
            vmm_map_page(pagemap, virt, phys, pf);
        }

        char *buf = (char *)((uintptr_t)addr + MEM_PHYS_OFFSET);
        ret = file->read(file, buf + misalign, phdr[i].p_offset, phdr[i].p_filesz);
        if (ret != phdr[i].p_filesz)
            goto out;
    }

    auxval->at_entry = base + hdr.entry;

    ok = true;

out:
    if (phdr != NULL)
        free(phdr);
    if (ld_path != NULL && *ld_path != NULL && ok == false)
        free(*ld_path);
    return ok;
}
