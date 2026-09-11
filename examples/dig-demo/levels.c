/**
 * @file levels.c
 * @brief Implementation of level_load() (see levels.h).
 */
#include "levels.h"
#include "tiles.h"

/**
 * @brief See level_load() in the header for the full contract.
 *
 * One hand-built demo level: a border of steel, dirt everywhere else, a
 * handful of rocks/diamonds, one butterfly, a small amoeba patch, and
 * an exit tile (unused by this simplified demo - see main.c).
 */
void level_load(int index)
{
    (void)index;

    for (int y = 0; y < MAP_H; y++) {
        for (int x = 0; x < MAP_W; x++) {
            if (x == 0 || y == 0 || x == MAP_W - 1 || y == MAP_H - 1)
                set_tile(x, y, T_STEEL);
            else
                set_tile(x, y, T_DIRT);
        }
    }

    set_tile(2, 2, T_PLAYER_SPAWN);

    /* Rocks */
    set_tile(5, 3, T_ROCK);
    set_tile(6, 3, T_ROCK);
    set_tile(10, 5, T_ROCK);
    set_tile(14, 4, T_ROCK);

    /* Diamonds */
    set_tile(3, 8, T_DIAMOND);
    set_tile(8, 9, T_DIAMOND);
    set_tile(12, 8, T_DIAMOND);
    set_tile(16, 10, T_DIAMOND);
    set_tile(6, 12, T_DIAMOND);

    /* A couple of internal steel obstacles */
    set_tile(9, 6, T_STEEL);
    set_tile(9, 7, T_STEEL);

    /* One butterfly enemy */
    set_tile(15, 3, T_BUTTERFLY_UP);

    /* Small amoeba patch */
    set_tile(4, 11, T_AMOEBA);
    set_tile(5, 11, T_AMOEBA);

    /* Exit - closed by default */
    set_tile(MAP_W - 3, MAP_H - 2, T_EXIT_CLOSED);
}
