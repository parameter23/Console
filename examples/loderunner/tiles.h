/**
 * @file tiles.h
 * @brief Tile map storage and tile IDs for the Lode Runner-style game.
 */
#pragma once
#include <stdint.h>

/**
 * @brief Tile IDs used by the map. T_PLAYER_SPAWN/T_GUARD_SPAWN are
 *        level-authoring markers only - levels.c reads them once at
 *        load time to place the runner/guards, then clears them to
 *        T_EMPTY, so get_tile() never returns them during play.
 */
typedef enum {
    T_EMPTY = 0,
    T_BRICK,          /* diggable - see try_dig() in main.c */
    T_STEEL,          /* indestructible wall/border */
    T_LADDER,         /* climbable with UP/DOWN */
    T_GOLD,           /* collectible; walking onto one picks it up */
    T_HOLE,           /* a dug, temporary pit - see holes[] in main.c */
    T_EXIT_CLOSED,    /* becomes T_EXIT_OPEN once all gold is collected */
    T_EXIT_OPEN,
    T_PLAYER_SPAWN,
    T_GUARD_SPAWN,
    T_MAX
} tile_id_t;

#define MAP_W 20
#define MAP_H 15

/** @brief The tile map, [row][col], values are tile_id_t. */
extern uint8_t map_data[MAP_H][MAP_W];

/**
 * @brief Reads a tile. Out-of-bounds coordinates read as T_STEEL, so
 *        movement/support checks don't need explicit bounds checks.
 * @param x, y Tile coordinates.
 * @return The tile_id_t at (x, y).
 */
uint8_t get_tile(int x, int y);

/**
 * @brief Writes a tile. Out-of-bounds coordinates are silently ignored.
 * @param x, y Tile coordinates.
 * @param t    New tile_id_t.
 */
void set_tile(int x, int y, uint8_t t);
