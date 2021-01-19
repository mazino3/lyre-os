#ifndef __DEV__DEV_H__
#define __DEV__DEV_H__

#include <stdbool.h>
#include <lib/types.h>
#include <lib/resource.h>

dev_t dev_new_id(void);

bool dev_add_new(struct resource *device, const char *dev_name);

#endif
