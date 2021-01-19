#include <stddef.h>
#include <stdint.h>
#include <dev/dev.h>
#include <fs/devtmpfs.h>
#include <lib/lock.h>

static dev_t device_id_counter = 1;

dev_t dev_new_id(void) {
    static lock_t lock = {0};
    SPINLOCK_ACQUIRE(lock);
    dev_t new_id = device_id_counter++;
    LOCK_RELEASE(lock);
    return new_id;
}

bool dev_add_new(struct resource *device, const char *dev_name) {
    device->st.st_rdev = dev_new_id();

    devtmpfs_add_device(device, dev_name);

    return true;
}
