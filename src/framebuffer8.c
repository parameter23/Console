/**
 * @file framebuffer8.c
 * @brief Implementation of the 8-bit paletted framebuffer (see framebuffer8.h).
 */
#include "framebuffer8.h"
#include "st7789.h"
#include "spi.h"
#include "libc_stubs.h"

uint8_t  framebuffer8[FB8_SIZE];
uint16_t fb8_palette[256];

/** @brief See fb8_clear() in the header for the full contract. */
void fb8_clear(uint8_t color)
{
    /* memset() is our own freestanding implementation (libc_stubs.c),
     * not libc - the project links with -nostdlib. */
    memset(framebuffer8, color, FB8_SIZE);
}

/** @brief See fb8_set_pixel() in the header for the full contract. */
void fb8_set_pixel(int x, int y, uint8_t color)
{
    if (x < 0 || y < 0 || x >= FB8_WIDTH || y >= FB8_HEIGHT)
        return;
    framebuffer8[y * FB8_WIDTH + x] = color;
}

/** @brief See fb8_get_pixel() in the header for the full contract. */
uint8_t fb8_get_pixel(int x, int y)
{
    if (x < 0 || y < 0 || x >= FB8_WIDTH || y >= FB8_HEIGHT)
        return 0;
    return framebuffer8[y * FB8_WIDTH + x];
}

/** @brief See fb8_fill_rect() in the header for the full contract. */
void fb8_fill_rect(int x, int y, int w, int h, uint8_t color)
{
    for (int yy = y; yy < y + h; yy++) {
        if (yy < 0 || yy >= FB8_HEIGHT) continue;
        for (int xx = x; xx < x + w; xx++) {
            if (xx < 0 || xx >= FB8_WIDTH) continue;
            framebuffer8[yy * FB8_WIDTH + xx] = color;
        }
    }
}

/** @brief See fb8_init_palette() in the header for the full contract. */
void fb8_init_palette(void)
{
    /* C64-style base palette in the first 16 slots.
     *
     * Indices 3/9/10/11/12/13/15 used to be filled with the same hex
     * digit repeated across all three RGB565 fields (e.g. 0x4444 for
     * "Dark Grey"). Because RGB565 splits unevenly (5/6/5 bits), a
     * repeated-digit value does *not* give equal R/G/B brightness - it
     * skews green, so those seven entries actually rendered as
     * olive/mint/mauve instead of their labelled colour. Fixed by
     * picking each colour's intended 8-bit RGB and re-encoding it
     * properly; the other nine entries already rendered correctly and
     * are unchanged. */
    static const uint16_t base_palette[16] = {
        0x0000, /* 0: Black       */
        0xFFFF, /* 1: White       */
        0x8800, /* 2: Red         */
        0x0639, /* 3: Cyan        */
        0xC897, /* 4: Purple      */
        0x04A8, /* 5: Green       */
        0x0015, /* 6: Blue        */
        0xEEE7, /* 7: Yellow      */
        0xDD86, /* 8: Orange      */
        0x6222, /* 9: Brown       */
        0xFB2C, /* 10: Light Red  */
        0x4228, /* 11: Dark Grey  */
        0x8C51, /* 12: Grey       */
        0x8FF1, /* 13: Light Green*/
        0x777F, /* 14: Light Blue */
        0xBDD7  /* 15: Light Grey */
    };

    for (int i = 0; i < 16; i++)
        fb8_palette[i] = base_palette[i];

    for (int i = 16; i < 256; i++)
        fb8_palette[i] = 0x0000;
}

/** @brief See fb8_flush() in the header for the full contract. */
void fb8_flush(void)
{
    st7789_set_window(0, 0, FB8_WIDTH - 1, FB8_HEIGHT - 1);

    CS_LOW();
    DC_DATA();

    for (int i = 0; i < FB8_SIZE; i++) {
        uint16_t c = fb8_palette[framebuffer8[i]];
        spi1_write8(c >> 8);
        spi1_write8(c & 0xFF);
    }

    CS_HIGH();
}

/** @brief One display line worth of RGB565 bytes, built from the palette
 *         and pushed out via DMA - much faster than fb8_flush(). */
static uint8_t fb8_linebuf[FB8_WIDTH * 2];

/** @brief See fb8_flush_dma() in the header for the full contract. */
void fb8_flush_dma(void)
{
    st7789_set_window(0, 0, FB8_WIDTH - 1, FB8_HEIGHT - 1);

    CS_LOW();
    DC_DATA();

    for (int y = 0; y < FB8_HEIGHT; y++) {
        int fb_index = y * FB8_WIDTH;
        int out = 0;

        for (int x = 0; x < FB8_WIDTH; x++) {
            uint16_t c = fb8_palette[framebuffer8[fb_index++]];
            fb8_linebuf[out++] = c >> 8;
            fb8_linebuf[out++] = c & 0xFF;
        }

        spi1_write_buf_dma(fb8_linebuf, FB8_WIDTH * 2);
    }

    CS_HIGH();
}
