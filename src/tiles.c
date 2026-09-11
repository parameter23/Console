#include "tiles.h"

uint8_t map_data[MAP_H][MAP_W];

uint8_t get_tile(int x, int y)
{
    if (x < 0 || y < 0 || x >= MAP_W || y >= MAP_H)
        return T_STEEL; // Out-of-bounds = Steel
    return map_data[y][x];
}

void set_tile(int x, int y, uint8_t t)
{
    if (x < 0 || y < 0 || x >= MAP_W || y >= MAP_H)
        return;
    map_data[y][x] = t;
}
