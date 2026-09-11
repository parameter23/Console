/**
 * @file sprite16.h
 * @brief The engine's single 16x16 paletted sprite format and its blitter.
 */
#ifndef GRAPHICS_SPRITE16_H
#define GRAPHICS_SPRITE16_H

#include <stdint.h>

/**
 * @brief A 16x16 sprite, 1 byte/pixel = palette index into fb8_palette[].
 *        Index 0 is transparent.
 *
 * This is the one and only sprite format used across the engine. (The
 * old Sprite4 4-bit-packed format (sprite.h) and the separate
 * sprite_tiles[SPRITE_COUNT][128] format (sprites.h) have been removed -
 * they duplicated this and were not used consistently.)
 */
typedef struct {
    uint8_t px[16][16];
} sprite16_t;

/**
 * @brief Draws a sprite16_t into the framebuffer, clipped to its bounds.
 *        Palette index 0 is skipped (transparent).
 * @param x    Framebuffer column of the sprite's top-left corner.
 * @param y    Framebuffer row of the sprite's top-left corner.
 * @param spr  Sprite to draw.
 */
void draw_sprite16(int x, int y, const sprite16_t *spr);

#endif
