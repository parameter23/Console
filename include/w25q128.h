/**
 * @file w25q128.h
 * @brief Minimal driver for the W25Q128 SPI NOR flash on the BlackPill's
 *        onboard SOI8 footprint (16MB/128Mbit, 4096 x 4KB sectors,
 *        256-byte pages).
 *
 * That footprint is hard-wired to CS=PA4/SCK=PA5/MISO=PA6/MOSI=PA7 -
 * the same SPI1 bus the display uses. Sharing SCK/MISO/MOSI between
 * two slaves is fine; each just needs its own CS, which is why the
 * display's CS moved to PB0 (see st7789.h) and left PA4 free here.
 *
 * There's no bus arbitration in software beyond "don't do two things
 * at once" - fine as-is because everything in this engine runs from a
 * single bare-metal main loop with nothing else touching SPI1
 * concurrently. Don't call these functions from an interrupt handler,
 * and don't start a flash operation while a display flush is still in
 * flight.
 *
 * Call w25q_init() only after SPI1 is already running (e.g.
 * st7789_init() has already run, same as sfx_init() assumes for
 * TIM3/TIM2) - it does not call spi1_setup() itself.
 */
#ifndef W25Q128_H
#define W25Q128_H

#include <stdint.h>
#include <libopencm3/stm32/gpio.h>

#define W25Q_CS_PORT   GPIOA
#define W25Q_CS_PIN    GPIO4

#define W25Q_PAGE_SIZE    256u
#define W25Q_SECTOR_SIZE  4096u
#define W25Q_TOTAL_SIZE   (16u * 1024u * 1024u)  /* 128Mbit = 16MB */

/**
 * @brief Configures the flash's dedicated CS pin (SPI1 itself must
 *        already be set up - see the file header). Call once.
 */
void w25q_init(void);

/**
 * @brief Reads the 3-byte JEDEC ID (manufacturer, memory type,
 *        capacity). A genuine W25Q128 reports 0xEF 0x40 0x18 - useful
 *        to confirm the chip is actually present/wired before trusting
 *        anything else.
 * @param out Destination for exactly 3 bytes.
 */
void w25q_read_id(uint8_t out[3]);

/**
 * @brief Reads len bytes starting at addr into buf. Plain sequential
 *        read - no alignment or length restriction.
 * @param addr Byte offset into the flash (0 .. W25Q_TOTAL_SIZE-1).
 * @param buf  Destination buffer.
 * @param len  Number of bytes to read.
 */
void w25q_read(uint32_t addr, uint8_t *buf, uint32_t len);

/**
 * @brief Erases one 4KB sector (every byte in it becomes 0xFF). Blocks
 *        until the erase completes - can take tens of milliseconds,
 *        see the datasheet.
 * @param addr Any byte address inside the sector to erase.
 */
void w25q_erase_sector(uint32_t addr);

/**
 * @brief Programs up to one page (256 bytes) of already-erased flash.
 *        Programming can only clear bits (1->0), never set them back
 *        to 1 - call w25q_erase_sector() first for any bytes that
 *        need to change from a previous write. Does not wrap at a page
 *        boundary; callers writing more than 256 bytes, or a range
 *        that crosses a page boundary, must split the write
 *        themselves. Blocks until done.
 * @param addr Start address.
 * @param buf  Data to write.
 * @param len  Number of bytes, 1..256.
 */
void w25q_write_page(uint32_t addr, const uint8_t *buf, uint32_t len);

#endif
