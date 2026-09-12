/**
 * @file levels.c
 * @brief Layouts for the three levels (see levels.h).
 *
 * Every level is bordered by T_STEEL and solvable by walking/climbing
 * alone (no digging required to reach any gold or the exit) - digging
 * is purely a tool against the guards. Verified offline with a small
 * Python simulator of the same movement rules main.c implements
 * (gravity, ladder climbing, brick/steel blocking) before being
 * transcribed here, so every gold piece and the exit are reachable.
 */
#include "levels.h"
#include "tiles.h"

LevelInfo level_info;

/** @brief Fills the whole map with empty tiles and a T_STEEL border. */
static void clear_with_border(void)
{
    for (int y = 0; y < MAP_H; y++) {
        for (int x = 0; x < MAP_W; x++) {
            int border = (x == 0 || y == 0 || x == MAP_W - 1 || y == MAP_H - 1);
            set_tile(x, y, border ? T_STEEL : T_EMPTY);
        }
    }
}

static void place(const int coords[][2], int count, tile_id_t tile)
{
    for (int i = 0; i < count; i++)
        set_tile(coords[i][0], coords[i][1], tile);
}

/** @brief Level 1 (easy): four full-width floors, one guard. */
static void build_level1(void)
{
    clear_with_border();

    static const int bricks[][2] = {
        {1,3},{2,3},{3,3},{5,3},{6,3},{7,3},{8,3},{9,3},{10,3},{11,3},{12,3},{13,3},{14,3},{16,3},{17,3},{18,3},
        {1,6},{2,6},{3,6},{4,6},{5,6},{6,6},{8,6},{9,6},{10,6},{11,6},{13,6},{14,6},{15,6},{16,6},{17,6},{18,6},
        {1,9},{2,9},{3,9},{5,9},{6,9},{7,9},{8,9},{9,9},{10,9},{11,9},{12,9},{13,9},{14,9},{16,9},{17,9},{18,9},
        {1,12},{2,12},{3,12},{4,12},{5,12},{6,12},{7,12},{8,12},{9,12},{10,12},{11,12},{12,12},{13,12},{14,12},{15,12},{16,12},{17,12},{18,12},
    };
    static const int ladders[][2] = {
        {4,2},{15,2},{4,3},{15,3},{4,4},{15,4},{4,5},{7,5},{12,5},{15,5},
        {7,6},{12,6},{7,7},{12,7},{4,8},{7,8},{12,8},{15,8},{4,9},{15,9},
        {4,10},{15,10},{4,11},{15,11},
    };
    static const int gold[][2] = {
        {2,2},{17,2},{2,5},{9,5},{17,5},{9,8},{17,8},{2,11},{17,11},
    };

    place(bricks, 66, T_BRICK);
    place(ladders, 24, T_LADDER);
    place(gold, 9, T_GOLD);
    set_tile(9, 2, T_EXIT_CLOSED);

    level_info.gold_total = 9;
    level_info.player_start_x = 9;  level_info.player_start_y = 11;
    level_info.guard_count = 1;
    level_info.guard_start_x[0] = 2; level_info.guard_start_y[0] = 8;
}

/** @brief Level 2 (medium): middle floors split in two, two guards. */
static void build_level2(void)
{
    clear_with_border();

    static const int bricks[][2] = {
        {1,3},{2,3},{3,3},{5,3},{6,3},{7,3},{8,3},{9,3},{10,3},{11,3},{12,3},{13,3},{14,3},{16,3},{17,3},{18,3},
        {1,6},{2,6},{3,6},{5,6},{6,6},{7,6},{8,6},{11,6},{12,6},{13,6},{14,6},{16,6},{17,6},{18,6},
        {1,9},{2,9},{3,9},{5,9},{6,9},{7,9},{8,9},{11,9},{12,9},{13,9},{14,9},{16,9},{17,9},{18,9},
        {1,12},{2,12},{3,12},{4,12},{5,12},{6,12},{7,12},{8,12},{9,12},{10,12},{11,12},{12,12},{13,12},{14,12},{15,12},{16,12},{17,12},{18,12},
    };
    static const int ladders[][2] = {
        {4,2},{15,2},{4,3},{15,3},{4,4},{15,4},{4,5},{15,5},{4,6},{15,6},
        {4,7},{15,7},{4,8},{15,8},{4,9},{15,9},{4,10},{15,10},{4,11},{15,11},
    };
    static const int gold[][2] = {
        {6,2},{2,5},{13,5},{17,5},{2,8},{6,8},{13,8},{17,11},
    };

    place(bricks, 62, T_BRICK);
    place(ladders, 20, T_LADDER);
    place(gold, 8, T_GOLD);
    set_tile(13, 2, T_EXIT_CLOSED);

    level_info.gold_total = 8;
    level_info.player_start_x = 2;  level_info.player_start_y = 11;
    level_info.guard_count = 2;
    level_info.guard_start_x[0] = 6;  level_info.guard_start_y[0] = 5;
    level_info.guard_start_x[1] = 17; level_info.guard_start_y[1] = 8;
}

/** @brief Level 3 (hard): five floors, two split, three guards. */
static void build_level3(void)
{
    clear_with_border();

    static const int bricks[][2] = {
        {1,2},{2,2},{3,2},{5,2},{6,2},{7,2},{8,2},{9,2},{10,2},{11,2},{12,2},{13,2},{14,2},{16,2},{17,2},{18,2},
        {1,5},{2,5},{3,5},{5,5},{6,5},{7,5},{12,5},{13,5},{14,5},{16,5},{17,5},{18,5},
        {1,8},{2,8},{3,8},{5,8},{6,8},{7,8},{8,8},{9,8},{10,8},{11,8},{12,8},{13,8},{14,8},{16,8},{17,8},{18,8},
        {1,11},{2,11},{3,11},{5,11},{6,11},{7,11},{12,11},{13,11},{14,11},{16,11},{17,11},{18,11},
        {1,13},{2,13},{3,13},{4,13},{5,13},{6,13},{7,13},{8,13},{9,13},{10,13},{11,13},{12,13},{13,13},{14,13},{15,13},{16,13},{17,13},{18,13},
    };
    static const int ladders[][2] = {
        {4,1},{15,1},{4,2},{15,2},{4,3},{15,3},{4,4},{15,4},{4,5},{15,5},
        {4,6},{15,6},{4,7},{15,7},{4,8},{15,8},{4,9},{15,9},{4,10},{15,10},
        {4,11},{15,11},{4,12},{15,12},
    };
    static const int gold[][2] = {
        {6,1},{13,1},{2,4},{6,4},{17,4},{2,7},{17,7},{2,10},{6,10},{13,10},{17,12},
    };

    place(bricks, 74, T_BRICK);
    place(ladders, 24, T_LADDER);
    place(gold, 11, T_GOLD);
    set_tile(9, 1, T_EXIT_CLOSED);

    level_info.gold_total = 11;
    level_info.player_start_x = 2;  level_info.player_start_y = 12;
    level_info.guard_count = 3;
    level_info.guard_start_x[0] = 13; level_info.guard_start_y[0] = 4;
    level_info.guard_start_x[1] = 9;  level_info.guard_start_y[1] = 7;
    level_info.guard_start_x[2] = 17; level_info.guard_start_y[2] = 10;
}

/** @brief See level_load() in the header for the full contract. */
void level_load(int index)
{
    switch (index) {
    case 0: build_level1(); break;
    case 1: build_level2(); break;
    default: build_level3(); break;
    }
}
