#include <stdint.h>
#include <stddef.h>
#include <dev/e9.h>
#include <dev/dev.h>
#include <sys/port_io.h>
#include <lib/resource.h>

static ssize_t e9_write(struct resource *this, const void *buf, off_t off, size_t count) {
    for (size_t i = 0; i < count; i++) {
        outb(0xe9, ((char*)buf)[i]);
    }

    return count;
}

bool e9_init(void) {
    struct resource *e9 = resource_create(sizeof(struct resource));

    e9->write = e9_write;
    e9->st.st_mode = S_IFCHR | 0666;

    dev_add_new(e9, "e9");

    return true;
}
