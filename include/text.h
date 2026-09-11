/**
 * @file text.h
 * @brief Fixed-width 8x8 bitmap text rendering into the framebuffer.
 */
#ifndef GRAPHICS_TEXT_H
#define GRAPHICS_TEXT_H

#include <stdint.h>

/**
 * @brief Draws one glyph from font8x8_basic. Characters outside the
 *        supported range (32..127) are rendered as '?'.
 * @param x     Top-left column.
 * @param y     Top-left row.
 * @param c     ASCII character to draw.
 * @param color Palette index.
 */
void draw_char(int x, int y, char c, uint8_t color);

/**
 * @brief Draws a NUL-terminated string left to right, 8 pixels per
 *        character, via draw_char().
 * @param x     Column of the first character.
 * @param y     Row of the first character.
 * @param s     NUL-terminated string.
 * @param color Palette index.
 */
void draw_text(int x, int y, const char *s, uint8_t color);

#endif
