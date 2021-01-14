#ifndef __LIB__ELF_H__
#define __LIB__ELF_H__

#include <stddef.h>

struct auxval_t {
    size_t at_entry;
    size_t at_phdr;
    size_t at_phent;
    size_t at_phnum;
};

#endif
