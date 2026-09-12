/**
 * @file spi.h
 * @brief Single, shared SPI1 driver used by the display and any other
 *        SPI peripheral.
 *
 * NOTE: In a previous version of this project, spi.c AND st7789.c each
 * configured SPI1 independently (with different baud-rate dividers).
 * That is fixed now: this is the ONLY place SPI1 is initialised.
 * Anything that talks to SPI1 must call spi1_setup() once and then use
 * spi1_write8()/spi1_write_buf()/spi1_xfer8()/spi1_read_buf().
 *
 * MISO (PA6) is wired up alongside SCK/MOSI so a second SPI1 slave with
 * its own CS - the W25Q128 flash on the BlackPill's onboard footprint,
 * see w25q128.h - can actually receive data back, not just the
 * display's write-only pixel pushes.
 */
#ifndef HAL_SPI_H
#define HAL_SPI_H

#include <stdint.h>

/**
 * @brief Configures and enables SPI1 as master, MSB-first, 8-bit frames.
 * @param baudrate_div One of libopencm3's SPI_CR1_BAUDRATE_FPCLK_DIV_2 .. _256.
 */
void spi1_setup(uint32_t baudrate_div);

/**
 * @brief Writes one byte, blocking until the transfer completes. The
 *        byte simultaneously clocked in on MISO is discarded - use
 *        spi1_xfer8() if the response matters (e.g. flash status/data).
 * @param data Byte to send.
 */
void spi1_write8(uint8_t data);

/**
 * @brief Full-duplex single-byte transfer: sends data and returns
 *        whatever was simultaneously clocked in on MISO.
 * @param data Byte to send.
 * @return Byte received.
 */
uint8_t spi1_xfer8(uint8_t data);

/**
 * @brief Writes a buffer one byte at a time via spi1_write8().
 * @param buf Bytes to send.
 * @param len Number of bytes.
 */
void spi1_write_buf(const uint8_t *buf, uint32_t len);

/**
 * @brief Reads a buffer one byte at a time, sending 0xFF as filler for
 *        each byte clocked out (the usual convention for reading from
 *        an SPI slave that only drives MISO in response to a command).
 * @param buf Destination for the received bytes.
 * @param len Number of bytes.
 */
void spi1_read_buf(uint8_t *buf, uint32_t len);

/**
 * @brief DMA-based bulk write (blocking until the transfer completes).
 *        Used by the framebuffer flush for much higher throughput than
 *        spi1_write_buf().
 * @param buf Bytes to send.
 * @param len Number of bytes.
 */
void spi1_write_buf_dma(const uint8_t *buf, uint32_t len);

#endif
