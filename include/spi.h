#ifndef HAL_SPI_H
#define HAL_SPI_H

#include <stdint.h>

/*
 * Single, shared SPI1 driver.
 *
 * NOTE: In the previous version of this project, spi.c AND st7789.c each
 * configured SPI1 independently (with different baud-rate dividers). That
 * is fixed here: this is now the ONLY place SPI1 is initialised. Anything
 * that talks to SPI1 (display, future SPI peripherals) must call
 * spi1_setup() once and then use spi1_write8()/spi1_write_buf().
 */

/* baudrate_div: one of SPI_CR1_BAUDRATE_FPCLK_DIV_2 .. _256 (libopencm3) */
void spi1_setup(uint32_t baudrate_div);

void spi1_write8(uint8_t data);
void spi1_write_buf(const uint8_t *buf, uint32_t len);

/* DMA-based bulk write (blocking until transfer complete). Used by the
 * framebuffer flush for higher throughput than spi1_write_buf(). */
void spi1_write_buf_dma(const uint8_t *buf, uint32_t len);

#endif
