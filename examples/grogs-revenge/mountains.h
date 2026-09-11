/**
 * @file mountains.h
 * @brief The three mountainsides Thor must cross, in order.
 */
#pragma once

#define MOUNTAIN_COUNT 3

/**
 * @brief Per-mountain parameters: how many clams Peter's toll costs,
 *        and where Thor and Grog start.
 */
typedef struct {
    int clams_needed;
    int thor_start_x, thor_start_y;
    int grog_start_x, grog_start_y;
    int toll_x, toll_y;
} MountainInfo;

/** @brief The currently loaded mountain's parameters, overwritten by each mountain_load() call. */
extern MountainInfo mountain_info;

/**
 * @brief Builds mountain `index` (0-based) into the tile map and fills
 *        in mountain_info for it.
 * @param index 0..MOUNTAIN_COUNT-1.
 */
void mountain_load(int index);
