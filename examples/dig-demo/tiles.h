/**
 * @file tiles.h
 * @brief Tile map storage and tile IDs for the dig-demo game.
 */
#pragma once
#include <stdint.h>

/**
 * @brief Tile IDs used by the dig-demo map. Values beyond T_ROCK/
 *        T_DIAMOND/T_STEEL/T_PLAYER_SPAWN (butterfly, amoeba, magic
 *        wall, exit states) are defined for a fuller Boulder-Dash-style
 *        game but not all are acted on by this simplified demo - see
 *        main.c's try_move().
 */
typedef enum {
    T_EMPTY = 0,
    T_DIRT,
    T_ROCK,
    T_DIAMOND,
    T_STEEL,
    T_EXIT_CLOSED,
    T_EXIT_OPEN,
    T_PLAYER_SPAWN,
    T_EXPLOSION,
    T_MAGIC_WALL,
    T_AMOEBA,
    T_BUTTERFLY_UP,
    T_BUTTERFLY_LEFT,
    T_BUTTERFLY_DOWN,
    T_BUTTERFLY_RIGHT,
    T_MAX
} tile_id_t;

#define MAP_W 20
#define MAP_H 15

/** @brief The tile map, [row][col], values are tile_id_t. */
extern uint8_t map_data[MAP_H][MAP_W];

/**
 * @brief Reads a tile. Out-of-bounds coordinates read as T_STEEL, so
 *        map-edge collision checks don't need explicit bounds checks.
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
