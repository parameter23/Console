/**
 * @file libc_stubs.c
 * @brief Implementation of the freestanding memset()/memcpy() (see
 *        libc_stubs.h).
 */
#include "libc_stubs.h"

/** @brief See memset() in the header for the full contract. */
void *memset(void *dst, int value, size_t n)
{
    unsigned char *d = (unsigned char *)dst;
    unsigned char v = (unsigned char)value;
    for (size_t i = 0; i < n; i++)
        d[i] = v;
    return dst;
}

/** @brief See memcpy() in the header for the full contract. */
void *memcpy(void *dst, const void *src, size_t n)
{
    unsigned char *d = (unsigned char *)dst;
    const unsigned char *s = (const unsigned char *)src;
    for (size_t i = 0; i < n; i++)
        d[i] = s[i];
    return dst;
}
