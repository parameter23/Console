/**
 * @file tiles.h
 * @brief Tile map storage and tile IDs for the Grogs Revenge example.
 *
 * Unlike the Zauberschloss example, mountains are visited strictly in
 * order and never revisited, so there is only one active tile buffer -
 * mountain_load() (see mountains.h) simply rebuilds it in place for
 * the next mountain.
 */
#pragma once
#include <stdint.h>

/**
 * @brief Tile IDs used by a mountainside. Thor and Grog are moving
 *        entities drawn separately (see main.c), never tiles.
 */
typedef enum {
    T_EMPTY = 0,  /**< Floor. */
    T_WALL,       /**< Cliff edge - blocks movement. */
    T_ROCK,       /**< Boulder - blocks movement. */
    T_HOLE,       /**< Pit - stepping in costs a life (see main.c). */
    T_CLAM,       /**< MUSCHEL - collected automatically by walking onto it. */
    T_TOLL,       /**< Toll post; BTN pays it with enough clams to advance. */
    T_MAX
} tile_id_t;

#define MAP_W 20
#define MAP_H 14

/**
 * @brief Reads a tile. Out-of-bounds coordinates read as T_WALL (the
 *        mountainside's edge), so edge collision checks don't need
 *        explicit bounds checks.
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
