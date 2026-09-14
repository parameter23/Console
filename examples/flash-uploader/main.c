/**
 * @file main.c
 * @brief One-shot tool firmware: streams a file from the host machine
 *        (via ARM semihosting, see semihost.h) into the onboard W25Q128
 *        SPI flash at a fixed address, erasing and writing it, then
 *        reads it back and compares to catch any corruption before
 *        declaring success. Not a game - no display/GameAPI involved,
 *        this only needs SPI1 and the flash.
 *
 * UPLOAD_FILENAME and UPLOAD_FLASH_ADDR are compile-time constants (see
 * the Makefile's FILE=/ADDR= overrides) - this firmware is rebuilt and
 * reflashed per upload rather than taking runtime parameters, which
 * keeps this whole tool to a few dozen lines. Usage: see README.md in
 * this directory (the flashing/run step is NOT the usual `make flash`,
 * since semihosting needs the debugger to stay attached and servicing
 * requests instead of detaching after programming).
 *
 * Progress and errors are reported two ways: PC13 (onboard LED) for
 * at-a-glance status without a debugger console open, and semihosting's
 * SYS_WRITE0/SYS_WRITEC (OpenOCD prints these to its own log) for the
 * actual detail.
 */
#include "clock.h"
#include "spi.h"
#include "w25q128.h"
#include "semihost.h"

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/spi.h>

#ifndef UPLOAD_FILENAME
#define UPLOAD_FILENAME "asset.bin"
#endif
#ifndef UPLOAD_FLASH_ADDR
#define UPLOAD_FLASH_ADDR 0x000000u
#endif

#define CHUNK_SIZE  W25Q_PAGE_SIZE  /* 256 - matches w25q_write_page()'s limit */

#define RESULT_PORT  GPIOC
#define RESULT_PIN   GPIO13

static void led_init(void)
{
    rcc_periph_clock_enable(RCC_GPIOC);
    gpio_mode_setup(RESULT_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, RESULT_PIN);
    gpio_set_output_options(RESULT_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_2MHZ, RESULT_PIN);
    gpio_set(RESULT_PORT, RESULT_PIN); /* active-low: start off */
}

/** @brief Reports failure on the LED (fast blink) and hangs - the
 *         SYS_WRITE0 message the caller already printed has the detail. */
static void fail_forever(void)
{
    for (;;) {
        gpio_toggle(RESULT_PORT, RESULT_PIN);
        delay_ms(100);
    }
}

/** @brief Minimal unsigned-int-to-decimal-string - no sprintf under -nostdlib. */
static void utoa10(uint32_t v, char *out)
{
    char tmp[12];
    int i = 0;
    if (v == 0) {
        out[0] = '0';
        out[1] = 0;
        return;
    }
    while (v > 0 && i < 11) {
        tmp[i++] = (char)('0' + (v % 10));
        v /= 10;
    }
    int j = 0;
    while (i > 0)
        out[j++] = tmp[--i];
    out[j] = 0;
}

static uint32_t sector_align_down(uint32_t v)
{
    return v - (v % W25Q_SECTOR_SIZE);
}

static uint32_t sector_align_up(uint32_t v)
{
    uint32_t rem = v % W25Q_SECTOR_SIZE;
    return rem ? v + (W25Q_SECTOR_SIZE - rem) : v;
}

int main(void)
{
    clock_setup();
    systick_setup();
    led_init();

    spi1_setup(SPI_CR1_BAUDRATE_FPCLK_DIV_4);
    w25q_init();

    semihost_write0("flash-uploader: opening '" UPLOAD_FILENAME "'...\n");
    int32_t fh = semihost_open(UPLOAD_FILENAME, SEMIHOST_MODE_RB);
    if (fh < 0) {
        semihost_write0("ERROR: could not open file (check it exists in "
                         "OpenOCD's working directory, or use an absolute "
                         "path for UPLOAD_FILENAME)\n");
        fail_forever();
    }

    int32_t flen = semihost_flen(fh);
    if (flen < 0) {
        semihost_write0("ERROR: could not get file length\n");
        fail_forever();
    }

    char num[12];
    utoa10((uint32_t)flen, num);
    semihost_write0("file length: ");
    semihost_write0(num);
    semihost_write0(" bytes\n");

    uint32_t base = (uint32_t)UPLOAD_FLASH_ADDR;
    uint32_t erase_from = sector_align_down(base);
    uint32_t erase_to = sector_align_up(base + (uint32_t)flen);

    semihost_write0("erasing flash sectors...\n");
    for (uint32_t a = erase_from; a < erase_to; a += W25Q_SECTOR_SIZE)
        w25q_erase_sector(a);

    semihost_write0("writing");
    uint8_t buf[CHUNK_SIZE];
    uint32_t addr = base;
    int32_t remaining = flen;
    while (remaining > 0) {
        int32_t want = remaining < (int32_t)CHUNK_SIZE ? remaining : (int32_t)CHUNK_SIZE;
        int32_t got = semihost_read(fh, buf, want);
        if (got != want) {
            semihost_write0("\nERROR: short read from host file\n");
            fail_forever();
        }
        w25q_write_page(addr, buf, (uint32_t)got);
        addr += (uint32_t)got;
        remaining -= got;
        semihost_writec('.');
    }
    semihost_write0("\n");
    semihost_close(fh);

    /* Verify: reopen the same host file and compare it against what was
     * actually read back from the flash, chunk by chunk. Catches any
     * corruption (bad page write, wrong address math) immediately
     * instead of shipping a silently-broken asset. */
    semihost_write0("verifying...");
    fh = semihost_open(UPLOAD_FILENAME, SEMIHOST_MODE_RB);
    if (fh < 0) {
        semihost_write0("\nERROR: could not reopen file for verification\n");
        fail_forever();
    }

    uint8_t vbuf[CHUNK_SIZE];
    addr = base;
    remaining = flen;
    while (remaining > 0) {
        int32_t want = remaining < (int32_t)CHUNK_SIZE ? remaining : (int32_t)CHUNK_SIZE;
        int32_t got = semihost_read(fh, buf, want);
        if (got != want) {
            semihost_write0("\nERROR: short read from host file during verify\n");
            fail_forever();
        }
        w25q_read(addr, vbuf, (uint32_t)want);

        for (int32_t i = 0; i < want; i++) {
            if (buf[i] != vbuf[i]) {
                semihost_write0("\nERROR: mismatch at flash address 0x");
                /* Reuse num[] as a scratch hex buffer - good enough for a
                 * one-shot diagnostic, no need for a real hex formatter. */
                uint32_t v = addr + (uint32_t)i;
                for (int k = 7; k >= 0; k--) {
                    uint32_t nib = (v >> (k * 4)) & 0xF;
                    num[7 - k] = (char)(nib < 10 ? '0' + nib : 'A' + nib - 10);
                }
                num[8] = 0;
                semihost_write0(num);
                semihost_write0("\n");
                fail_forever();
            }
        }

        addr += (uint32_t)got;
        remaining -= got;
    }
    semihost_close(fh);

    semihost_write0(" OK\nflash-uploader: done.\n");
    gpio_clear(RESULT_PORT, RESULT_PIN); /* success: LED on solid */

    for (;;) {
        /* Hold here with the LED lit. */
    }
}
