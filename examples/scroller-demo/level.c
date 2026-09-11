/**
 * @file level.c
 * @brief The hand-built level layout (see level.h): a starting run-up,
 *        a couple of hills, three jumpable pits, a handful of spikes,
 *        and coins scattered along a safe path to the flag at GOAL_COL.
 */
#include "level.h"

uint8_t ground_row[LEVEL_W];
uint8_t obstacle_at[LEVEL_W];
uint8_t coin_at[LEVEL_W];

static void set_range(int from, int to, int height)
{
    for (int x = from; x <= to && x < LEVEL_W; x++)
        ground_row[x] = (uint8_t)height;
}

static void set_pit(int from, int to)
{
    set_range(from, to, MAP_H);
}

/** @brief See level_build() in the header for the full contract. */
void level_build(void)
{
    for (int x = 0; x < LEVEL_W; x++) {
        ground_row[x] = 11;
        obstacle_at[x] = 0;
        coin_at[x] = 0;
    }

    /* Small hill. */
    set_range(10, 10, 10);
    set_range(11, 13, 9);
    set_range(14, 14, 10);

    /* Gap 1 - must jump. */
    set_pit(20, 22);

    obstacle_at[27] = 1;

    /* Staircase hill. */
    set_range(30, 30, 10);
    set_range(31, 32, 9);
    set_range(33, 33, 8);
    set_range(34, 34, 9);
    set_range(35, 35, 10);

    /* Gap 2. */
    set_pit(38, 40);

    obstacle_at[44] = 1;
    obstacle_at[48] = 1;

    /* Another hill. */
    set_range(52, 52, 10);
    set_range(53, 55, 9);
    set_range(56, 56, 10);

    /* Gap 3. */
    set_pit(60, 62);

    obstacle_at[68] = 1;
    obstacle_at[72] = 1;

    /* Flat run-up to the flag. */
    set_range(85, LEVEL_W - 1, 11);

    static const int coins[] = {
        3, 6, 12, 18, 26, 33, 43, 49, 55, 66, 73, 86, 90
    };
    for (unsigned i = 0; i < sizeof(coins) / sizeof(coins[0]); i++)
        coin_at[coins[i]] = 1;
}
