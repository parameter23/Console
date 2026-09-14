/**
 * @file art.c
 * @brief Scene illustrations (see art.h) and the blitter that draws
 *        one at the top of the screen.
 */
#include "art.h"
#include "sprite16.h"
#include "framebuffer8.h"
#include "w25q128.h"

/* BG_HAUS: an overgrown old house between the trees - no photo
 * provided for this one, stays tile-drawn. */
static const uint8_t bg_haus[BG_ROWS][BG_COLS] = {
    {T_SKY_STAR,T_SKY_STAR,T_MOON,T_SKY_STAR,T_SKY_STAR,T_SKY_STAR,T_SKY_STAR,T_SKY_STAR,T_SKY_STAR,T_SKY_STAR,T_SKY_STAR,T_SKY_STAR,T_SKY_STAR,T_SKY_STAR,T_SKY_STAR,T_SKY_STAR,T_SKY_STAR,T_SKY_STAR,T_SKY_STAR,T_SKY_STAR},
    {T_TREE,T_TREE,T_TREE,T_TREE,T_TREE,T_TREE,T_ROOF,T_ROOF,T_ROOF,T_ROOF,T_ROOF,T_ROOF,T_ROOF,T_ROOF,T_TREE,T_TREE,T_TREE,T_TREE,T_TREE,T_TREE},
    {T_TREE,T_TREE,T_TREE,T_TREE,T_TREE,T_TREE,T_HOUSE_WALL,T_HOUSE_WALL,T_HOUSE_WALL,T_DOOR,T_HOUSE_WALL,T_HOUSE_WALL,T_HOUSE_WALL,T_HOUSE_WALL,T_TREE,T_TREE,T_TREE,T_TREE,T_TREE,T_TREE},
    {T_TREE,T_GROUND,T_GROUND,T_GROUND,T_GROUND,T_GROUND,T_GROUND,T_GROUND,T_GROUND,T_GROUND,T_GROUND,T_GROUND,T_GROUND,T_GROUND,T_GROUND,T_GROUND,T_GROUND,T_GROUND,T_GROUND,T_TREE},
    {T_GROUND,T_GROUND,T_GROUND,T_GROUND,T_GROUND,T_GROUND,T_GROUND,T_GROUND,T_GROUND,T_GROUND,T_GROUND,T_GROUND,T_GROUND,T_GROUND,T_GROUND,T_GROUND,T_GROUND,T_GROUND,T_GROUND,T_GROUND},
};

/* Only BG_HAUS still uses a tile grid (no photo provided for it); the
 * rest are photo-backed now, so this table is deliberately sparse -
 * bg_table[bg] is only ever read when bg_flash_slot[bg] is -1. */
static const uint8_t (*const bg_table[BG_COUNT])[BG_COLS] = {
    [BG_HAUS] = bg_haus,
};

/* Each slot is one 320x240 raw image (FB8_WIDTH*FB8_HEIGHT bytes,
 * framebuffer8[] layout) on the external W25Q128, rounded up to the
 * next 4KB sector boundary - see tools/img2fullscreen.py --format raw
 * and examples/flash-uploader/. Placed at KOBOLD_FLASH_BASE, well clear
 * of nebelkrone's images (which start at 0 and run to roughly 0x85000),
 * since both examples share the one physical flash chip. */
#define KOBOLD_FLASH_BASE 0x100000u
#define BG_IMG_SLOT_SIZE  (((FB8_WIDTH * FB8_HEIGHT + 4095u) / 4096u) * 4096u)

/* Slot index per bg_id_t, or -1 to fall back to the tile-based
 * bg_table[] illustration above. Upload order (slot 0..6) must be:
 * Heide.jpg, Waldweg2.jpg, Kobold_weint.jpg, Moor.jpg, Frau.jpg,
 * Kobold_kampf.jpg, Bach.jpg. */
static const int8_t bg_flash_slot[BG_COUNT] = {
    [BG_WALD]   = 1,  /* Waldweg2.jpg */
    [BG_HAUS]   = -1, /* no photo - tile fallback */
    [BG_KOBOLD] = 2,  /* Kobold_weint.jpg */
    [BG_MOOR]   = 3,  /* Moor.jpg */
    [BG_BACH]   = 6,  /* Bach.jpg */
    [BG_PFAD]   = 0,  /* Heide.jpg */
    [BG_WIN]    = 4,  /* Frau.jpg */
    [BG_ATTACK] = 5,  /* Kobold_kampf.jpg */
};

/** @brief See draw_scene_bg() in the header for the full contract. */
void draw_scene_bg(bg_id_t bg)
{
    int8_t slot = bg_flash_slot[bg];
    if (slot >= 0) {
        w25q_read(KOBOLD_FLASH_BASE + (uint32_t)slot * BG_IMG_SLOT_SIZE,
                   framebuffer8, (uint32_t)FB8_WIDTH * (uint32_t)FB8_HEIGHT);
        return;
    }

    const uint8_t (*grid)[BG_COLS] = bg_table[bg];

    for (int row = 0; row < BG_ROWS; row++)
        for (int col = 0; col < BG_COLS; col++)
            draw_sprite16(col * 16, row * 16, &tileset16[grid[row][col]]);
}
