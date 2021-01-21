#include <stdint.h>
#include <stddef.h>
#include <misc/initramfs.h>
#include <dev/dev.h>
#include <stivale/stivale2.h>
#include <mm/vmm.h>
#include <lib/print.h>
#include <fs/vfs.h>
#include <lib/builtins.h>
#include <lib/math.h>

struct ustar_header {
    char name[100];
    char mode[8];
    char uid[8];
    char gid[8];
    char size[12];
    char mtime[12];
    char checksum[8];
    uint8_t type;
    char link_name[100];
    char signature[6];
    char version[2];
    char owner[32];
    char group[32];
    char device_maj[8];
    char device_min[8];
    char prefix[155];
};

enum {
    USTAR_FILE = '0',
    USTAR_HARD_LINK = '1',
    USTAR_SYM_LINK = '2',
    USTAR_CHAR_DEV = '3',
    USTAR_BLOCK_DEV = '4',
    USTAR_DIR = '5',
    USTAR_FIFO = '6'
};

static uintptr_t initramfs_addr;
static uintptr_t initramfs_size;

static uint64_t octal_to_int(const char *s) {
    uint64_t ret = 0;
    while (*s) {
        ret *= 8;
        ret += *s - '0';
        s++;
    }
    return ret;
}

bool initramfs_init(struct stivale2_struct_tag_modules *modules_tag) {
    if (modules_tag->module_count < 1) {
        print("No initramfs!\n");
        for (;;);
    }

    struct stivale2_module *module = &modules_tag->modules[0];

    initramfs_addr = module->begin + MEM_PHYS_OFFSET;
    initramfs_size = module->end - module->begin;

    print("initramfs: Address: %X\n", initramfs_addr);
    print("initramfs: Size:    %U\n", initramfs_size);

    struct ustar_header *h = (void*)initramfs_addr;
    for (;;) {
        if (strncmp(h->signature, "ustar", 5) != 0)
            break;

        uintptr_t size = octal_to_int(h->size);

        switch (h->type) {
            case USTAR_DIR: {
                vfs_mkdir(NULL, h->name, octal_to_int(h->mode), false);
                break;
            }
            case USTAR_FILE: {
                struct resource *r = vfs_open(h->name, O_RDWR | O_CREAT,
                                              octal_to_int(h->mode));
                void *buf = (void*)h + 512;
                r->write(r, buf, 0, size);
                r->close(r);
                break;
            }
        }

        h = (void*)h + 512 + ALIGN_UP(size, 512);

        if ((uintptr_t)h >= initramfs_addr + initramfs_size)
            break;
    }

    print("initramfs: Imported into VFS\n");

    pmm_free(initramfs_addr - MEM_PHYS_OFFSET,
             DIV_ROUNDUP(initramfs_size, PAGE_SIZE));

    print("initramfs: Reclaimed %U pages of memory\n",
          DIV_ROUNDUP(initramfs_size, PAGE_SIZE));

    return true;
}
