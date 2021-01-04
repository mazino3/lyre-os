#include <stddef.h>
#include <stdint.h>
#include <dev/dev.h>

static dev_t device_id_counter = 0;

dev_t dev_new_id(void) {
    return device_id_counter++;
}
