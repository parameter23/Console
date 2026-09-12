/**
 * @file levels.h
 * @brief The three levels the runner must clear, in order.
 */
#pragma once

#define LEVEL_COUNT 3
#define MAX_GUARDS  3

/**
 * @brief Per-level parameters: how much gold to collect before the exit
 *        opens, where the runner starts, and where each guard starts.
 */
typedef struct {
    int gold_total;
    int player_start_x, player_start_y;
    int guard_count;
    int guard_start_x[MAX_GUARDS];
    int guard_start_y[MAX_GUARDS];
} LevelInfo;

/** @brief The currently loaded level's parameters, overwritten by each level_load() call. */
extern LevelInfo level_info;

/**
 * @brief Builds level `index` (0-based) into the tile map and fills in
 *        level_info for it.
 * @param index 0..LEVEL_COUNT-1.
 */
void level_load(int index);
