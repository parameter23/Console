/**
 * @file framebuffer8.h
 * @brief 8-bit paletted framebuffer (320x240) and its display flush.
 */
#ifndef DISPLAY_FRAMEBUFFER8_H
#define DISPLAY_FRAMEBUFFER8_H

#include <stdint.h>

#define FB8_WIDTH   320
#define FB8_HEIGHT  240
#define FB8_SIZE    (FB8_WIDTH * FB8_HEIGHT)

/** @brief 1 byte per pixel = palette index into fb8_palette[]. */
extern uint8_t  framebuffer8[FB8_SIZE];
/** @brief 256-entry RGB565 palette, index 0 used as "transparent" by sprites. */
extern uint16_t fb8_palette[256];

/**
 * @brief Fills the whole framebuffer with one palette index.
 * @param color Palette index to fill with.
 */
void fb8_clear(uint8_t color);

/**
 * @brief Sets one pixel. Out-of-bounds coordinates are silently ignored.
 * @param x     Pixel column.
 * @param y     Pixel row.
 * @param color Palette index.
 */
void fb8_set_pixel(int x, int y, uint8_t color);

/**
 * @brief Reads one pixel. Out-of-bounds coordinates return 0.
 * @param x Pixel column.
 * @param y Pixel row.
 * @return Palette index at (x, y).
 */
uint8_t fb8_get_pixel(int x, int y);

/**
 * @brief Fills an axis-aligned rectangle with one palette index.
 * @param x, y  Top-left corner.
 * @param w, h  Width and height in pixels.
 * @param color Palette index.
 */
void fb8_fill_rect(int x, int y, int w, int h, uint8_t color);

/**
 * @brief Loads the full 256-color palette: the C64-style named colors
 *        into fb8_palette[0..15] (every existing sprite/tile only ever
 *        references these), and a 6x6x6 RGB color cube + 24-step
 *        grayscale ramp into fb8_palette[16..255] for quantizing
 *        photographic/continuous-tone art (see tools/img2fullscreen.py,
 *        which mirrors this same extended palette). Call once before
 *        drawing anything.
 */
void fb8_init_palette(void);

/**
 * @brief Pushes the whole framebuffer to the display, one pixel at a time
 *        over polled SPI. fb8_flush_dma() is much faster and should be
 *        preferred; this is kept as a simple polled fallback/reference
 *        implementation.
 */
void fb8_flush(void);

/**
 * @brief Pushes the whole framebuffer to the display one scanline at a
 *        time via DMA. The normal way to present a frame.
 */
void fb8_flush_dma(void);

#endif
