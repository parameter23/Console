/**
 * @file tiles.c
 * @brief Implementation of the tile map accessors (see tiles.h).
 */
#include "tiles.h"

static uint8_t map_data[MAP_H][MAP_W];

/** @brief See get_tile() in the header for the full contract. */
uint8_t get_tile(int x, int y)
{
    if (x < 0 || y < 0 || x >= MAP_W || y >= MAP_H)
        return T_WALL;
    return map_data[y][x];
}

/** @brief See set_tile() in the header for the full contract. */
void set_tile(int x, int y, uint8_t t)
{
    if (x < 0 || y < 0 || x >= MAP_W || y >= MAP_H)
        return;
    map_data[y][x] = t;
}
