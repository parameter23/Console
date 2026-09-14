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

    /* Indices 16-255: a 6x6x6 RGB colour cube (levels 0/51/102/153/204/255
     * per channel, the classic "web-safe" spacing) followed by a 24-step
     * grayscale ramp - 216 + 24 = 240 entries. General-purpose coverage
     * for quantizing photographic/continuous-tone art (see
     * tools/img2fullscreen.py, which mirrors this exact palette) rather
     * than named/curated colours like the first 16. Indices 0-15 are
     * unchanged, so every existing sprite/tile (all of which only ever
     * reference those) renders identically to before. */
    static const uint16_t ext_palette[240] = {
        0x0000, 0x0006, 0x000C, 0x0013, 0x0019, 0x001F, 0x01A0, 0x01A6,
        0x01AC, 0x01B3, 0x01B9, 0x01BF, 0x0320, 0x0326, 0x032C, 0x0333,
        0x0339, 0x033F, 0x04C0, 0x04C6, 0x04CC, 0x04D3, 0x04D9, 0x04DF,
        0x0640, 0x0646, 0x064C, 0x0653, 0x0659, 0x065F, 0x07E0, 0x07E6,
        0x07EC, 0x07F3, 0x07F9, 0x07FF, 0x3000, 0x3006, 0x300C, 0x3013,
        0x3019, 0x301F, 0x31A0, 0x31A6, 0x31AC, 0x31B3, 0x31B9, 0x31BF,
        0x3320, 0x3326, 0x332C, 0x3333, 0x3339, 0x333F, 0x34C0, 0x34C6,
        0x34CC, 0x34D3, 0x34D9, 0x34DF, 0x3640, 0x3646, 0x364C, 0x3653,
        0x3659, 0x365F, 0x37E0, 0x37E6, 0x37EC, 0x37F3, 0x37F9, 0x37FF,
        0x6000, 0x6006, 0x600C, 0x6013, 0x6019, 0x601F, 0x61A0, 0x61A6,
        0x61AC, 0x61B3, 0x61B9, 0x61BF, 0x6320, 0x6326, 0x632C, 0x6333,
        0x6339, 0x633F, 0x64C0, 0x64C6, 0x64CC, 0x64D3, 0x64D9, 0x64DF,
        0x6640, 0x6646, 0x664C, 0x6653, 0x6659, 0x665F, 0x67E0, 0x67E6,
        0x67EC, 0x67F3, 0x67F9, 0x67FF, 0x9800, 0x9806, 0x980C, 0x9813,
        0x9819, 0x981F, 0x99A0, 0x99A6, 0x99AC, 0x99B3, 0x99B9, 0x99BF,
        0x9B20, 0x9B26, 0x9B2C, 0x9B33, 0x9B39, 0x9B3F, 0x9CC0, 0x9CC6,
        0x9CCC, 0x9CD3, 0x9CD9, 0x9CDF, 0x9E40, 0x9E46, 0x9E4C, 0x9E53,
        0x9E59, 0x9E5F, 0x9FE0, 0x9FE6, 0x9FEC, 0x9FF3, 0x9FF9, 0x9FFF,
        0xC800, 0xC806, 0xC80C, 0xC813, 0xC819, 0xC81F, 0xC9A0, 0xC9A6,
        0xC9AC, 0xC9B3, 0xC9B9, 0xC9BF, 0xCB20, 0xCB26, 0xCB2C, 0xCB33,
        0xCB39, 0xCB3F, 0xCCC0, 0xCCC6, 0xCCCC, 0xCCD3, 0xCCD9, 0xCCDF,
        0xCE40, 0xCE46, 0xCE4C, 0xCE53, 0xCE59, 0xCE5F, 0xCFE0, 0xCFE6,
        0xCFEC, 0xCFF3, 0xCFF9, 0xCFFF, 0xF800, 0xF806, 0xF80C, 0xF813,
        0xF819, 0xF81F, 0xF9A0, 0xF9A6, 0xF9AC, 0xF9B3, 0xF9B9, 0xF9BF,
        0xFB20, 0xFB26, 0xFB2C, 0xFB33, 0xFB39, 0xFB3F, 0xFCC0, 0xFCC6,
        0xFCCC, 0xFCD3, 0xFCD9, 0xFCDF, 0xFE40, 0xFE46, 0xFE4C, 0xFE53,
        0xFE59, 0xFE5F, 0xFFE0, 0xFFE6, 0xFFEC, 0xFFF3, 0xFFF9, 0xFFFF,
        0x0000, 0x0861, 0x18A3, 0x2104, 0x2965, 0x39C7, 0x4228, 0x4A69,
        0x5ACB, 0x632C, 0x6B6D, 0x7BCF, 0x8430, 0x9492, 0x9CD3, 0xA534,
        0xB596, 0xBDD7, 0xC638, 0xD69A, 0xDEFB, 0xE75C, 0xF79E, 0xFFFF,
    };

    for (int i = 0; i < 240; i++)
        fb8_palette[16 + i] = ext_palette[i];
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
