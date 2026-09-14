/**
 * @file semihost.c
 * @brief Implementation of the ARM semihosting file I/O helpers (see
 *        semihost.h). All operations go through a single BKPT 0xAB trap -
 *        the standard semihosting entry point on Cortex-M (ARM's SVC-based
 *        encoding is A32-only; Thumb targets like this one always use
 *        BKPT 0xAB instead) - with the operation number in r0 and a
 *        pointer to its parameter block (or, for SYS_WRITE0/SYS_WRITEC,
 *        the single parameter itself) in r1. The result comes back in r0.
 */
#include "semihost.h"

#define SYS_OPEN    0x01
#define SYS_CLOSE   0x02
#define SYS_WRITEC  0x03
#define SYS_WRITE0  0x04
#define SYS_READ    0x06
#define SYS_FLEN    0x0C

static int32_t semihost_call(int32_t op, const void *arg)
{
    register int32_t     r0 __asm__("r0") = op;
    register const void *r1 __asm__("r1") = arg;
    __asm__ volatile (
        "bkpt 0xAB\n"
        : "+r"(r0)
        : "r"(r1)
        : "memory"
    );
    return r0;
}

/** @brief See semihost_open() in the header for the full contract. */
int32_t semihost_open(const char *filename, int32_t mode)
{
    int32_t len = 0;
    while (filename[len])
        len++;

    int32_t block[3] = { (int32_t)(intptr_t)filename, mode, len };
    return semihost_call(SYS_OPEN, block);
}

/** @brief See semihost_read() in the header for the full contract. */
int32_t semihost_read(int32_t handle, void *buf, int32_t len)
{
    int32_t block[3] = { handle, (int32_t)(intptr_t)buf, len };
    int32_t not_read = semihost_call(SYS_READ, block);
    return len - not_read;
}

/** @brief See semihost_close() in the header for the full contract. */
int32_t semihost_close(int32_t handle)
{
    int32_t block[1] = { handle };
    return semihost_call(SYS_CLOSE, block);
}

/** @brief See semihost_flen() in the header for the full contract. */
int32_t semihost_flen(int32_t handle)
{
    int32_t block[1] = { handle };
    return semihost_call(SYS_FLEN, block);
}

/** @brief See semihost_write0() in the header for the full contract. */
void semihost_write0(const char *s)
{
    semihost_call(SYS_WRITE0, s);
}

/** @brief See semihost_writec() in the header for the full contract. */
void semihost_writec(char c)
{
    semihost_call(SYS_WRITEC, &c);
}
