/**
 * @file tiles.c
 * @brief Implementation of the tile map accessors (see tiles.h).
 */
#include "tiles.h"
#include <stddef.h>

static uint8_t (*current_map)[MAP_W] = NULL;

/** @brief See tiles_select_map() in the header for the full contract. */
void tiles_select_map(uint8_t (*room_map)[MAP_W])
{
    current_map = room_map;
}

/** @brief See get_tile() in the header for the full contract. */
uint8_t get_tile(int x, int y)
{
    if (x < 0 || y < 0 || x >= MAP_W || y >= MAP_H || !current_map)
        return T_WALL;
    return current_map[y][x];
}

/** @brief See set_tile() in the header for the full contract. */
void set_tile(int x, int y, uint8_t t)
{
    if (x < 0 || y < 0 || x >= MAP_W || y >= MAP_H || !current_map)
        return;
    current_map[y][x] = t;
}
