/**
 * @file mountains.c
 * @brief Layouts for the three mountains (see mountains.h).
 *
 * Each mountain is a single self-contained screen bordered by cliff
 * walls (T_WALL) - there is no room-to-room travel here, unlike
 * Zauberschloss. Reaching the next mountain happens by paying Peter's
 * toll at the T_TOLL post once enough clams have been collected.
 */
#include "mountains.h"
#include "tiles.h"

MountainInfo mountain_info;

/** @brief Fills the whole map with floor and a T_WALL border. */
static void clear_with_border(void)
{
    for (int y = 0; y < MAP_H; y++) {
        for (int x = 0; x < MAP_W; x++) {
            int border = (x == 0 || y == 0 || x == MAP_W - 1 || y == MAP_H - 1);
            set_tile(x, y, border ? T_WALL : T_EMPTY);
        }
    }
}

static void place(const int coords[][2], int count, tile_id_t tile)
{
    for (int i = 0; i < count; i++)
        set_tile(coords[i][0], coords[i][1], tile);
}

/** @brief Mountain 1 (easy): 6 rocks, 4 holes, 8 clams, toll costs 6. */
static void build_mountain1(void)
{
    clear_with_border();

    static const int rocks[][2]  = { {5,3}, {8,5}, {12,4}, {6,9}, {10,10}, {14,8} };
    static const int holes[][2]  = { {4,7}, {9,3}, {13,11}, {7,11} };
    static const int clams[][2]  = { {3,2}, {6,4}, {9,8}, {11,3}, {15,5}, {4,10}, {12,11}, {16,9} };

    place(rocks, 6, T_ROCK);
    place(holes, 4, T_HOLE);
    place(clams, 8, T_CLAM);

    set_tile(17, 7, T_TOLL);

    mountain_info.clams_needed = 6;
    mountain_info.thor_start_x = 2;  mountain_info.thor_start_y = 7;
    mountain_info.grog_start_x = 17; mountain_info.grog_start_y = 2;
    mountain_info.toll_x = 17;       mountain_info.toll_y = 7;
}

/** @brief Mountain 2 (medium): 10 rocks, 7 holes, 10 clams, toll costs 8. */
static void build_mountain2(void)
{
    clear_with_border();

    static const int rocks[][2] = {
        {4,3}, {7,3}, {10,4}, {13,3}, {5,6}, {9,6}, {13,6}, {6,10}, {10,11}, {14,10}
    };
    static const int holes[][2] = { {3,9}, {8,2}, {12,9}, {16,4}, {6,4}, {11,10}, {15,2} };
    static const int clams[][2] = {
        {2,3}, {2,10}, {5,2}, {8,9}, {11,2}, {14,5}, {4,11}, {9,10}, {16,7}, {17,10}
    };

    place(rocks, 10, T_ROCK);
    place(holes, 7, T_HOLE);
    place(clams, 10, T_CLAM);

    set_tile(17, 7, T_TOLL);

    mountain_info.clams_needed = 8;
    mountain_info.thor_start_x = 2;  mountain_info.thor_start_y = 7;
    mountain_info.grog_start_x = 10; mountain_info.grog_start_y = 2;
    mountain_info.toll_x = 17;       mountain_info.toll_y = 7;
}

/** @brief Mountain 3 (hard): 14 rocks, 9 holes, 12 clams, toll costs 10. */
static void build_mountain3(void)
{
    clear_with_border();

    static const int rocks[][2] = {
        {3,3}, {6,3}, {9,3}, {12,3}, {15,3}, {4,6}, {7,6}, {10,6}, {13,6}, {16,6},
        {5,9}, {8,9}, {11,9}, {14,9}
    };
    static const int holes[][2] = {
        {2,4}, {5,2}, {8,2}, {11,2}, {14,2}, {3,11}, {6,11}, {9,11}, {12,11}
    };
    static const int clams[][2] = {
        {2,2}, {17,2}, {2,11}, {17,11}, {6,8}, {9,8}, {12,8}, {15,8},
        {4,4}, {7,4}, {10,4}, {13,4}
    };

    place(rocks, 14, T_ROCK);
    place(holes, 9, T_HOLE);
    place(clams, 12, T_CLAM);

    set_tile(17, 7, T_TOLL);

    mountain_info.clams_needed = 10;
    mountain_info.thor_start_x = 2; mountain_info.thor_start_y = 7;
    mountain_info.grog_start_x = 5; mountain_info.grog_start_y = 4;
    mountain_info.toll_x = 17;      mountain_info.toll_y = 7;
}

/** @brief See mountain_load() in the header for the full contract. */
void mountain_load(int index)
{
    switch (index) {
    case 0: build_mountain1(); break;
    case 1: build_mountain2(); break;
    default: build_mountain3(); break;
    }
}
