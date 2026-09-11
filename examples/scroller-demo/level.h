/**
 * @file level.h
 * @brief The scrolling level: a per-column ground-height field plus
 *        obstacle/coin overlays, wider than the screen. This is what
 *        makes the demo a real side-scroller rather than a room grid -
 *        the camera moves through it in single pixels, not whole tiles.
 */
#pragma once
#include <stdint.h>
#include "tiles.h"

#define LEVEL_W    100  /**< Level width in tiles (1600 px). */
#define MAP_H      14   /**< Screen height in tiles (224 px; 16 px left for the HUD). */
#define GOAL_COL   96   /**< Tile column of the flag; reaching it wins. */

/**
 * @brief Row (0..MAP_H) at which solid ground starts in column x; rows
 *        below it are ground (T_GRASS on top, T_DIRT beneath), rows
 *        above are open air. MAP_H itself means "no ground here" (a pit).
 */
extern uint8_t ground_row[LEVEL_W];

/** @brief 1 where a spike sits on top of the ground in that column. */
extern uint8_t obstacle_at[LEVEL_W];

/** @brief 1 where an uncollected coin floats in that column; cleared on pickup. */
extern uint8_t coin_at[LEVEL_W];

/**
 * @brief Builds the level into ground_row[]/obstacle_at[]/coin_at[].
 *        Call once at game start and again to reset after death.
 */
void level_build(void);
