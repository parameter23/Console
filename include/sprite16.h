#ifndef GRAPHICS_SPRITE16_H
#define GRAPHICS_SPRITE16_H

#include <stdint.h>

/*
 * The one and only sprite format used across the engine now.
 * 16x16 pixels, 1 byte/pixel = palette index into fb8_palette[].
 * Index 0 is transparent.
 *
 * (The old Sprite4 4-bit-packed format (sprite.h) and the separate
 * sprite_tiles[SPRITE_COUNT][128] format (sprites.h) have been removed -
 * they duplicated this and were not used consistently.)
 */
typedef struct {
    uint8_t px[16][16];
} sprite16_t;

void draw_sprite16(int x, int y, const sprite16_t *spr);

#endif
