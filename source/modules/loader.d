import modules.mod;

immutable uint PT_LOAD     = 0x00000001;
immutable uint PT_INTERP   = 0x00000003;
immutable uint PT_PHDR     = 0x00000006;

immutable uint ABI_SYSV    = 0x00;
immutable uint ARCH_X86_64 = 0x3e;
immutable uint ARCH_X86_32 = 0x03;
immutable uint BITS_LE     = 0x01;
immutable uint ET_DYN      = 0x0003;
immutable uint SHT_RELA    = 0x00000004;
immutable uint R_X86_64_RELATIVE = 0x00000008;

/* Indices into identification array */
immutable uint EI_CLASS    = 4;
immutable uint EI_DATA     = 5;
immutable uint EI_VERSION  = 6;
immutable uint EI_OSABI    = 7;

struct elf64_hdr {
    ubyte[16]  ident;
    ushort type;
    ushort machine;
    uint vers;
    ulong entry;
    ulong phoff;
    ulong shoff;
    uint flags;
    ushort hdr_size;
    ushort phdr_size;
    ushort ph_num;
    ushort shdr_size;
    ushort sh_num;
    ushort shstrndx;
};

struct elf64_phdr {
    uint p_type;
    uint p_flags;
    ulong p_offset;
    ulong p_vaddr;
    ulong p_paddr;
    ulong p_filesz;
    ulong p_memsz;
    ulong p_align;
};

struct elf64_shdr {
    uint sh_name;
    uint sh_type;
    ulong sh_flags;
    ulong sh_addr;
    ulong sh_offset;
    ulong sh_size;
    uint sh_link;
    uint sh_info;
    ulong sh_addralign;
    ulong sh_entsize;
};

struct elf64_rela {
    ulong r_addr;
    uint r_info;
    uint r_symbol;
    long r_addend;
};

struct elf64_sym {
    uint st_name;
    ubyte st_info;
    ubyte st_other;
    ushort st_shndx;
    ulong st_value;
    ulong st_size;
}

__gshared extern (C) ulong kernelTop;
__gshared ulong bump_base;
import lib.messages;

void init_modules() {
    ulong base = cast(ulong)&kernelTop;
    bump_base = base + (base % 0x1000);
    log(bump_base);
    elf64_load();
}

void read(void* ptr, size_t off, size_t len) {
    import lib.glue;
    memcpy(ptr, &a_o[off], len);
}

ulong module_alloc(size_t num) {
    ulong alloc = bump_base;
    bump_base += num;
    return bump_base;
}

int elf64_load() {
    import lib.string;
    elf64_hdr hdr;
    read(&hdr, 0, elf64_hdr.sizeof);

    if(fromCString(cast(char *)hdr.ident, 4) != "\177ELF") {
        log("invalid elf sig ", fromCString(cast(char *)hdr.ident, 4));
        return false;
    }

    if (hdr.ident[EI_DATA] != BITS_LE) {
        return -1;
    }

    if (hdr.machine != ARCH_X86_64) {
        return -1;
    }

    struct loaded_section {
        string name;
        size_t address;
    }

    import lib.list;
    auto loaded_sections = List!(loaded_section)(1);

    //read section header string table
    elf64_shdr shst_shdr;
    read(&shst_shdr, hdr.shoff + hdr.shstrndx * elf64_shdr.sizeof, elf64_shdr.sizeof);
    import lib.alloc;
    char* section_names = newArray!(char)(shst_shdr.sh_size);
    log("off, size: ", shst_shdr.sh_offset, " ", shst_shdr.sh_size);
    read(section_names, shst_shdr.sh_offset, shst_shdr.sh_size);
    elf64_sym* symbol_table;
    size_t symtab_elements = 0;
    char* strtab;

    log("file valid ", hdr.sh_num);
    import lib.glue;
    for (ushort i = 0; i < hdr.sh_num; i++) {
        elf64_shdr shdr;
        read(&shdr, hdr.shoff + i * elf64_shdr.sizeof,
                    elf64_shdr.sizeof);
        size_t page_cnt = ((((shdr.sh_size) + (0x1000) - 1) / (0x1000)) * (0x1000));
        string name = fromCString(&section_names[shdr.sh_name]);
        if (shdr.sh_type == 1) {
            size_t base = module_alloc(page_cnt);
            log("section with base: ", base);
            read(cast(void*)base, shdr.sh_offset, shdr.sh_size);
            log("section name: ", name);
            loaded_sections.push(loaded_section(name, base));
        } else if (shdr.sh_type == 8) {
            size_t base = module_alloc(page_cnt);
            memset(cast(void*)base, 0, shdr.sh_size);
            loaded_sections.push(loaded_section(name, base));
        } else if(shdr.sh_type == 2) {
            log("found symtab");
            log("section name: ", name, " ", shdr.sh_size, " ",elf64_sym.sizeof);
            symbol_table = newArray!(elf64_sym)(shdr.sh_size / elf64_sym.sizeof);
            symtab_elements = shdr.sh_size / elf64_sym.sizeof;
            read(symbol_table, shdr.sh_offset, shdr.sh_size);
            loaded_sections.push(loaded_section(name, 0));
        } else if(shdr.sh_type == 3) {
            if(name == ".strtab") {
                log("found strtab");
                log("section name: ", name, " ", shdr.sh_offset);
                strtab = newArray!(char)(shdr.sh_size);
                read(strtab, shdr.sh_offset, shdr.sh_size);
                loaded_sections.push(loaded_section(name, 0));
            }
        } else {
            loaded_sections.push(loaded_section(name, 0));
        }
    }

    string name_ = fromCString(&strtab[1]);
    log(" ", name_);
    log(" ");
    log(" ");
    log(" ");
    for (ushort i = 0; i < hdr.sh_num; i++) {
        elf64_shdr shdr;
        read(&shdr, hdr.shoff + i * elf64_shdr.sizeof,
                    elf64_shdr.sizeof);
        if(shdr.sh_type == 4) {
            string name = fromCString(&section_names[shdr.sh_name]);
            string relocated_name = fromCString(&section_names[shdr.sh_name + 5]);
            elf64_rela* relocations = newArray!(elf64_rela)(shdr.sh_size / (elf64_rela.sizeof));
            log("section name: ", name, " ", relocated_name);
            read(cast(void*)relocations, shdr.sh_offset, shdr.sh_size);
            for(int j = 0; j < (shdr.sh_size / (elf64_rela.sizeof)); j++) {
                size_t location = relocations[j].r_addr;
                size_t info = relocations[j].r_info;
                size_t symbol_idx = relocations[j].r_symbol;
                long addend = relocations[j].r_addend;
                elf64_sym symbol = symbol_table[symbol_idx];
                elf64_shdr sec_shdr;
                read(&sec_shdr, hdr.shoff + symbol.st_shndx * elf64_shdr.sizeof, elf64_shdr.sizeof);
                string _name = fromCString(&section_names[sec_shdr.sh_name]);
                string symname = fromCString(&strtab[symbol.st_name]);
                log("relocation for section ", loaded_sections[symbol.st_shndx].name, " ", symname);

                size_t relocated_base = 0;
                for(int k = 0; k < loaded_sections.length; k++) {
                    auto section = loaded_sections[k];
                    if(section.name == relocated_name) {
                        relocated_base = section.address;
                    }
                }

                size_t patch_address = location + relocated_base;
                //the value of the symbol we have to patch in
                size_t symbol_value = loaded_sections[symbol.st_shndx].address + symbol.st_value;
                log("relocating ", patch_address, " with ", symbol_value, " rel type: ", info);
                if(info == 4 || info == 2) {
                    uint* val = cast(uint*)patch_address;
                    import core.volatile;
                    volatileStore(val, cast(uint)(symbol_value + addend - patch_address));
                }
            }
        }
    }
    size_t entry = 0;
    for(size_t i = 0; i < symtab_elements; i++) {
        auto symbol = symbol_table[i];
        string symname = fromCString(&strtab[symbol.st_name]);
        if(symname == "entry") {
            log("entry point found");
            entry = loaded_sections[symbol.st_shndx].address + symbol.st_value;
            break;
        }
    }
    log("entry point at: ", entry);
    void function() fp = cast(void function())entry;
    fp();
    log("done loading");

    return 0;
}
