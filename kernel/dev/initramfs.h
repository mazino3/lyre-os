#ifndef __DEV__INITRAMFS_H__
#define __DEV__INITRAMFS_H__

#include <stdbool.h>
#include <stivale/stivale2.h>

bool initramfs_init(struct stivale2_struct_tag_modules *modules_tag);

#endif
