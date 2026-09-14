/**
 * @file art.h
 * @brief Tile art and pre-composed scene illustrations for "Der Kobold" -
 *        inherited tile vocabulary from gamebook-template, five new
 *        illustrations.
 *
 * Every illustration is a 20x5 grid of the engine's normal 16x16
 * sprite16_t tiles (320x80 px, the top third of the screen) - the same
 * tile-grid technique dig-demo uses for its room map, just aimed at
 * static backdrops instead of a walkable map.
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
    BG_COUNT
} bg_id_t;

/**
 * @brief Draws one pre-composed illustration at the top of the screen
 *        (0,0)..(320,80).
 * @param bg Which illustration to draw.
 */
void draw_scene_bg(bg_id_t bg);

#endif
