/**
 * @file semihost.h
 * @brief Minimal ARM semihosting file I/O - lets firmware read a file
 *        that lives on the HOST machine (the one running the debugger),
 *        streamed over the existing SWD/OpenOCD link. Only works while
 *        a debugger has semihosting enabled and is actively resumed on
 *        the target (e.g. OpenOCD's `arm semihosting enable` before
 *        `resume`) - without one attached, the BKPT this uses faults
 *        instead of returning. See examples/flash-uploader/README.md.
 */
#ifndef SEMIHOST_H
#define SEMIHOST_H

#include <stdint.h>

#define SEMIHOST_MODE_RB  1   /* fopen-style "rb" - see SYS_OPEN in the
                                * semihosting spec for the full mode table */

/**
 * @brief Opens a file on the host via SYS_OPEN.
 * @param filename Null-terminated host-side path (resolved relative to
 *                  wherever the debugger/OpenOCD process itself runs).
 * @param mode      A SEMIHOST_MODE_* constant.
 * @return A file handle >=0, or -1 on failure.
 */
int32_t semihost_open(const char *filename, int32_t mode);

/**
 * @brief Reads up to len bytes from an open semihosting file handle.
 * @return Number of bytes actually read (0..len); less than len means
 *         end-of-file was reached partway through the request.
 */
int32_t semihost_read(int32_t handle, void *buf, int32_t len);

/** @brief Closes a semihosting file handle. @return 0 on success. */
int32_t semihost_close(int32_t handle);

/** @brief Returns an open file's length in bytes, or -1 on failure. */
int32_t semihost_flen(int32_t handle);

/** @brief Prints a null-terminated string to the debugger's console. */
void semihost_write0(const char *s);

/** @brief Prints a single character to the debugger's console. */
void semihost_writec(char c);

#endif
