/**
 * @file art.h
 * @brief Scene illustrations for "Der Kobold" - every station and
 *        ending now loads a full-screen 320x240 photo from the
 *        external W25Q128 flash at runtime (see bg_flash_slot[] in
 *        art.c and tools/img2fullscreen.py --format raw), the same
 *        technique nebelkrone uses.
 */
#ifndef KOBOLD_ART_H
#define KOBOLD_ART_H

#include <stdint.h>

/**
 * @brief Selects which pre-composed illustration a scene shows. Order
 *        matches story.h's StationType exactly, so draw_scene_bg() can
 *        be called directly as draw_scene_bg((bg_id_t)station).
 */
typedef enum {
    BG_WALD = 0,
    BG_HAUS,
    BG_KOBOLD,
    BG_MOOR,
    BG_BACH,
    BG_PFAD,
    BG_WIN,     /* the kobold, ring given: her curse breaks (ending) */
    BG_ATTACK,  /* the kobold, attacked: she strikes back (ending) */
    BG_COUNT
} bg_id_t;

/**
 * @brief Draws the scene's full-screen illustration (see art.c's
 *        bg_flash_slot[]).
 * @param bg Which illustration to draw.
 */
void draw_scene_bg(bg_id_t bg);

#endif
