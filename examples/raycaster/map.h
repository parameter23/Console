/**
 * @file map.h
 * @brief The raycaster demo's world: a small fixed 2D wall grid plus the
 *        palette colors used to shade each wall type.
 */
#ifndef RAYCASTER_MAP_H
#define RAYCASTER_MAP_H

#include <stdint.h>

#define MAP_W 16
#define MAP_H 16

/** @brief 0 = open floor, 1..3 = wall type (index into wall_colors + 1). */
extern const uint8_t world_map[MAP_H][MAP_W];

/**
 * @brief Per-wall-type shading: [wall type - 1][side], side 0 = a N/S-facing
 *        wall face, side 1 = an E/W-facing face. The E/W shade is always
 *        the darker of the pair - the classic cheap raycaster lighting
 *        trick (no real light source, just orientation contrast).
 */
extern const uint8_t wall_colors[3][2];

#endif
