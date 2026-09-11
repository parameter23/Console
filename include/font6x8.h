/**
 * @file font6x8.h
 * @brief 6x8 bitmap font table, ASCII 32..127 (currently unused by the
 *        engine's text.c, which uses font8x8.h - kept as an alternate,
 *        narrower font resource).
 */
#ifndef FONT6X8_H
#define FONT6X8_H

#include <stdint.h>

/** @brief One row per byte, 6 bytes/glyph, indexed by (ascii - 32). */
extern const uint8_t font6x8[96][6];

#endif
