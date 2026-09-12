/**
 * @file framebuffer8.h
 * @brief 4-bit paletted framebuffer (480x320) and its display flush.
 */
#ifndef DISPLAY_FRAMEBUFFER8_H
#define DISPLAY_FRAMEBUFFER8_H

#include <stdint.h>

#define FB8_WIDTH   480
#define FB8_HEIGHT  320
/** @brief 2 pixels per byte (4bpp) - same total RAM as the old 320x240
 *         8bpp buffer, just spread over the bigger 480x320 panel. */
#define FB8_SIZE    ((FB8_WIDTH * FB8_HEIGHT) / 2)

/** @brief Packed 4-bit palette indices, 2 pixels/byte (low nibble = even
 *         x, high nibble = odd x). Use fb8_set_pixel()/fb8_get_pixel(),
 *         don't index this directly. */
extern uint8_t  framebuffer8[FB8_SIZE];
/** @brief 16-entry RGB565 palette, index 0 used as "transparent" by sprites. */
extern uint16_t fb8_palette[16];

/**
 * @brief Fills the whole framebuffer with one palette index.
 * @param color Palette index (0-15) to fill with.
 */
void fb8_clear(uint8_t color);

/**
 * @brief Sets one pixel. Out-of-bounds coordinates are silently ignored.
 * @param x     Pixel column.
 * @param y     Pixel row.
 * @param color Palette index (0-15).
 */
void fb8_set_pixel(int x, int y, uint8_t color);

/**
 * @brief Reads one pixel. Out-of-bounds coordinates return 0.
 * @param x Pixel column.
 * @param y Pixel row.
 * @return Palette index (0-15) at (x, y).
 */
uint8_t fb8_get_pixel(int x, int y);

/**
 * @brief Fills an axis-aligned rectangle with one palette index.
 * @param x, y  Top-left corner.
 * @param w, h  Width and height in pixels.
 * @param color Palette index (0-15).
 */
void fb8_fill_rect(int x, int y, int w, int h, uint8_t color);

/**
 * @brief Loads the default C64-style 16-color palette into fb8_palette[].
 *        Call once before drawing anything.
 */
void fb8_init_palette(void);

/**
 * @brief Pushes the whole framebuffer to the display, one pixel at a
 *        time over polled SPI. fb8_flush_dma() is much faster and
 *        should be preferred; this is kept as a simple polled
 *        fallback/reference implementation.
 */
void fb8_flush(void);

/**
 * @brief Pushes the whole framebuffer to the display one scanline at a
 *        time via DMA. The normal way to present a frame.
 */
void fb8_flush_dma(void);

#endif
