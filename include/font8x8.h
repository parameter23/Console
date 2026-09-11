/**
 * @file font8x8.h
 * @brief 8x8 bitmap font table, ASCII 32..127, used by text.c.
 */
#pragma once
#include <stdint.h>

/** @brief One row per byte (MSB = leftmost pixel), indexed by (ascii - 32). */
extern const uint8_t font8x8_basic[96][8];
