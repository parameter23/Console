/**
 * @file text.h
 * @brief Fixed-width 8x8 bitmap text rendering into the framebuffer.
 */
#ifndef GRAPHICS_TEXT_H
#define GRAPHICS_TEXT_H

#include <stdint.h>

/**
 * @brief Draws one glyph from font8x8_basic. The full byte range 0-255 is
 *        covered (Code Page 850 - see font8x8.c).
 * @param x     Top-left column.
 * @param y     Top-left row.
 * @param c     Character to draw (a raw CP850 byte value, not UTF-8).
 * @param color Palette index.
 */
void draw_char(int x, int y, char c, uint8_t color);

/**
 * @brief Draws a NUL-terminated string left to right, 8 pixels per
 *        character, via draw_char(). Accepts UTF-8: a multi-byte
 *        sequence is decoded and, if its code point has a Code Page 850
 *        glyph (e.g. any German umlaut/ß, most other Western European
 *        accented letters, box-drawing/block characters), drawn as one
 *        8px-wide character - so a source file's own literal "ä" just
 *        works, no ASCII substitution (e.g. "ae") needed anymore. A code
 *        point outside CP850 draws as '?'.
 * @param x     Column of the first character.
 * @param y     Row of the first character.
 * @param s     NUL-terminated, UTF-8-encoded string.
 * @param color Palette index.
 */
void draw_text(int x, int y, const char *s, uint8_t color);

#endif
