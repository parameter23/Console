#ifndef LIBC_STUBS_H
#define LIBC_STUBS_H

#include <stddef.h>

/*
 * Minimal freestanding replacements for the handful of libc functions
 * that are convenient to have. The project links with -nostdlib (see
 * Makefile), so there is no libc providing memset()/memcpy() - these are
 * the real implementations, not just a workaround for one call site.
 */
void *memset(void *dst, int value, size_t n);
void *memcpy(void *dst, const void *src, size_t n);

#endif
