/**
 * @file map.h
 * @brief The walkable world for kobold3d: a 1-cell-wide entrance corridor
 *        leading into the 3x3 forest room from "Der Kobold" (see story.h's
 *        FOREST_W/FOREST_H), bordered by a single "hedge" wall type.
 */
#ifndef KOBOLD3D_MAP_H
#define KOBOLD3D_MAP_H

#include <stdint.h>

#define MAP_W 7
#define MAP_H 8

/* Forest cell (fx,fy) (0..2 each, see story.h) maps to world_map cell
 * (FOREST_ORIGIN_X + fx, FOREST_ORIGIN_Y + fy). The corridor sits at
 * column FOREST_ORIGIN_X + ENTRANCE_X, rows 1..3, leading straight into
 * the entrance cell at row FOREST_ORIGIN_Y (row 4). */
#define FOREST_ORIGIN_X 2
#define FOREST_ORIGIN_Y 4

/** @brief 0 = open floor, 1..3 = wall type (index into wall_colors + 1).
 *         Only type 2 ("hedge") is actually used by this map. */
extern const uint8_t world_map[MAP_H][MAP_W];

/**
 * @brief Per-wall-type shading: [wall type - 1][side], side 0 = a N/S-facing
 *        wall face, side 1 = an E/W-facing face (same convention as
 *        examples/raycaster/map.h).
 */
extern const uint8_t wall_colors[3][2];

#endif
