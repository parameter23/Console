#ifndef DISPLAY_FRAMEBUFFER8_H
#define DISPLAY_FRAMEBUFFER8_H

#include <stdint.h>

#define FB8_WIDTH   320
#define FB8_HEIGHT  240
#define FB8_SIZE    (FB8_WIDTH * FB8_HEIGHT)

/* 1 byte per pixel = palette index into fb8_palette[] */
extern uint8_t  framebuffer8[FB8_SIZE];
extern uint16_t fb8_palette[256];   /* RGB565 */

void fb8_clear(uint8_t color);
void fb8_set_pixel(int x, int y, uint8_t color);
uint8_t fb8_get_pixel(int x, int y);
void fb8_fill_rect(int x, int y, int w, int h, uint8_t color);

void fb8_init_palette(void);

/* Push the whole framebuffer to the display. fb8_flush_dma() is much
 * faster and should be preferred; fb8_flush() is kept as a simple
 * polled fallback / reference implementation. */
void fb8_flush(void);
void fb8_flush_dma(void);

#endif
