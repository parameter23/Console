/**
 * @file libc_stubs.h
 * @brief Minimal freestanding replacements for the handful of libc
 *        functions that are convenient to have. The project links with
 *        -nostdlib (see Makefile), so there is no libc providing
 *        memset()/memcpy() - these are the real implementations, not
 *        just a workaround for one call site.
 */
#ifndef LIBC_STUBS_H
#define LIBC_STUBS_H

#include <stddef.h>

/**
 * @brief Fills a memory region with a repeated byte value.
 * @param dst   Destination buffer.
 * @param value Byte value to write (only the low 8 bits are used).
 * @param n     Number of bytes to write.
 * @return dst.
 */
void *memset(void *dst, int value, size_t n);

/**
 * @brief Copies a memory region. Behavior is undefined if src and dst
 *        overlap (no memmove()-style handling).
 * @param dst Destination buffer.
 * @param src Source buffer.
 * @param n   Number of bytes to copy.
 * @return dst.
 */
void *memcpy(void *dst, const void *src, size_t n);

#endif
