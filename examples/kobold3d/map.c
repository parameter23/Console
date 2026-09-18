/**
 * @file map.c
 * @brief World data for kobold3d (see map.h).
 */
#include "map.h"

/* Everything starts as hedge (2), then the corridor (x=3, y=1..3) and the
 * 3x3 room (x=2..4, y=4..6) are carved out as open floor - same
 * "wall-first" construction as examples/raycaster/map.c. */
const uint8_t world_map[MAP_H][MAP_W] = {
    { 2,2,2,2,2,2,2 },
    { 2,2,2,0,2,2,2 },
    { 2,2,2,0,2,2,2 },
    { 2,2,2,0,2,2,2 },
    { 2,2,0,0,0,2,2 },
    { 2,2,0,0,0,2,2 },
    { 2,2,0,0,0,2,2 },
    { 2,2,2,2,2,2,2 },
};

const uint8_t wall_colors[3][2] = {
    { 12, 11 }, /* type 1: unused here */
    { 13,  5 }, /* type 2: light green / green - the hedge */
    { 10,  2 }, /* type 3: unused here */
};
