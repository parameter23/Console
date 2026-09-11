/**
 * @file text.c
 * @brief Implementation of the 8x8 bitmap text renderer (see text.h).
 */
#include "text.h"
#include "font8x8.h"
#include "framebuffer8.h"

/** @brief See draw_char() in the header for the full contract. */
void draw_char(int x, int y, char c, uint8_t color)
{
    if (c < 32 || c > 127)
        c = '?';

    const uint8_t *glyph = font8x8_basic[(int)(c - 32)];

    for (int row = 0; row < 8; row++) {
        uint8_t bits = glyph[row];
        for (int col = 0; col < 8; col++) {
            if (bits & (1 << (7 - col)))   /* MSB = leftmost pixel */
                fb8_set_pixel(x + col, y + row, color);
        }
    }
}

/** @brief See draw_text() in the header for the full contract. */
void draw_text(int x, int y, const char *s, uint8_t color)
{
    while (*s) {
        draw_char(x, y, *s, color);
        x += 8;
        s++;
    }
}
