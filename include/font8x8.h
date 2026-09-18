/**
 * @file font8x8.h
 * @brief 8x8 bitmap font table, full Code Page 850 (0x00-0xFF), used by
 *        text.c.
 */
#pragma once
#include <stdint.h>

/** @brief One row per byte (MSB = leftmost pixel), indexed directly by the
 *         character's byte value (0-255) - no offset/range check needed,
 *         every possible byte has a glyph. */
extern const uint8_t font8x8_basic[256][8];
