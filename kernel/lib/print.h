#pragma once

#include <stddef.h>
#include <stdarg.h>

void print(const char *fmt, ...);
size_t snprint(char *buf, size_t limit, const char *fmt, ...);
size_t vsnprint(char *print_buf, size_t limit, const char *fmt, va_list args);
