#ifndef __DEV__DEV_H__
#define __DEV__DEV_H__

#include <stdbool.h>
#include <lib/types.h>
#include <lib/resource.h>

// The following functions were taken from mlibc

static inline unsigned int dev_major(
		unsigned long long int __dev) {
  return ((__dev >> 8) & 0xfff) | ((unsigned int)(__dev >> 32) & ~0xfff);
}

static inline unsigned int dev_minor(
		unsigned long long int __dev) {
  return (__dev & 0xff) | ((unsigned int)(__dev >> 12) & ~0xff);
}

static inline unsigned long long int dev_makedev(
		unsigned int __major, unsigned int __minor) {
  return ((__minor & 0xff) | ((__major & 0xfff) << 8)
	  | (((unsigned long long int)(__minor & ~0xff)) << 12)
	  | (((unsigned long long int)(__major & ~0xfff)) << 32));
}

dev_t dev_new_id(void);

bool dev_add_new(struct resource *device, const char *dev_name);

bool dev_init(void);

#endif
