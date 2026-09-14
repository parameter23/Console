/**
 * @file art.h
 * @brief Tile art and pre-composed scene illustrations for Die Nebelkrone.
 *
 * Every illustration is a 20x5 grid of the engine's normal 16x16
 * sprite16_t tiles (320x80 px, the top third of the screen) - the same
 * tile-grid technique dig-demo/zauberschloss/grogs-revenge use for their
 * room maps, just aimed at static backdrops instead of a walkable map.
 */
#ifndef NEBELKRONE_ART_H
#define NEBELKRONE_ART_H

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

/** @brief Selects which pre-composed illustration a scene shows. */
typedef enum {
    BG_VILLAGE = 0,
    BG_SWAMP,
    BG_RUIN,
    BG_RUIN_INSIDE,
    BG_RUIN_CHAMBER,
    BG_RUIN_TOP_FIGHT,
    BG_RUIN_TOP_WON,
    BG_DEATH,
    BG_COUNT
} bg_id_t;

/**
 * @brief Draws the scene's illustration, filling the whole framebuffer.
 *        Most scenes load a full-screen photo from the external
 *        W25Q128 flash (see art.c's bg_flash_slot[]); any bg_id_t
 *        without one falls back to the original tile-based grid at the
 *        top of the screen, (0,0)..(320,80), leaving the rest of the
 *        framebuffer as whatever fb8_clear() set it to.
 * @param bg Which illustration to draw.
 */
void draw_scene_bg(bg_id_t bg);

#endif
