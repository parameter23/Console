/**
 * @file art.c
 * @brief Scene illustrations (see art.h): every station and ending now
 *        loads a full-screen photo from the external W25Q128 flash.
 */
#include "art.h"
#include "framebuffer8.h"
#include "w25q128.h"

/* Each slot is one 320x240 raw image (FB8_WIDTH*FB8_HEIGHT bytes,
 * framebuffer8[] layout) on the external W25Q128, rounded up to the
 * next 4KB sector boundary - see tools/img2fullscreen.py --format raw
 * and examples/flash-uploader/. Placed at KOBOLD_FLASH_BASE, well clear
 * of nebelkrone's images (which start at 0 and run to roughly 0x85000),
 * since both examples share the one physical flash chip. */
#define KOBOLD_FLASH_BASE 0x100000u
#define BG_IMG_SLOT_SIZE  (((FB8_WIDTH * FB8_HEIGHT + 4095u) / 4096u) * 4096u)

/* Slot index per bg_id_t. Upload order (slot 0..7) must be:
 * Heide.jpg, Waldweg2.jpg, Kobold_weint.jpg, Moor.jpg, Frau.jpg,
 * Kobold_kampf.jpg, Bach.jpg, Hexenhaus2.jpg. */
static const uint8_t bg_flash_slot[BG_COUNT] = {
    [BG_WALD]   = 1,  /* Waldweg2.jpg */
    [BG_HAUS]   = 7,  /* Hexenhaus2.jpg */
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
    uint32_t slot = bg_flash_slot[bg];
    w25q_read(KOBOLD_FLASH_BASE + slot * BG_IMG_SLOT_SIZE,
               framebuffer8, (uint32_t)FB8_WIDTH * (uint32_t)FB8_HEIGHT);
}
