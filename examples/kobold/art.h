/**
 * @file art.h
 * @brief Scene illustrations for "Der Kobold". Most are now full-screen
 *        320x240 photos loaded from the external W25Q128 flash at
 *        runtime (see bg_flash_slot[] in art.c and
 *        tools/img2fullscreen.py --format raw) - the same technique
 *        nebelkrone uses. BG_HAUS (no photo provided) still falls back
 *        to the original 20x5 tile-grid technique dig-demo uses for its
 *        room map, filling only the top 320x80 band and leaving the
 *        rest of the screen whatever fb8_clear() set it to.
 */
#ifndef KOBOLD_ART_H
#define KOBOLD_ART_H

#include <stdint.h>
#include "sprite16.h"

/** @brief Tile IDs used by the illustration grids (see tileset16.c). */
typedef enum {
    T_SKY = 0,
    T_SKY_STAR,
    T_MOON,
    T_TREE,
    T_GROUND,
    T_PATH,
    T_WALL,
    T_WALL_TORCH,
    T_DOOR,
    T_ROOF,
    T_HOUSE_WALL,
    T_WATER,
    T_FOG,
    T_FLOOR,
    T_CHEST,
    T_MONSTER,
    T_CROWN,
    T_MAX
} tile_id_t;

/** @brief Pixel data for every tile_id_t (see tileset16.c). */
extern const sprite16_t tileset16[T_MAX];

#define BG_COLS 20
#define BG_ROWS 5

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
 * @brief Draws the scene's illustration, filling the whole framebuffer.
 *        Most scenes load a full-screen photo from the external
 *        W25Q128 flash (see art.c's bg_flash_slot[]); BG_HAUS falls
 *        back to the tile-based grid at the top of the screen,
 *        (0,0)..(320,80), leaving the rest of the framebuffer as
 *        whatever fb8_clear() set it to.
 * @param bg Which illustration to draw.
 */
void draw_scene_bg(bg_id_t bg);

#endif
