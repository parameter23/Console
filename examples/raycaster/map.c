/**
 * @file map.c
 * @brief World data for the raycaster demo (see map.h).
 */
#include "map.h"

/* A small bordered maze: outer wall (type 1) all around so a ray cast
 * from anywhere inside always hits something within a few steps, plus
 * two interior wall clusters (types 2 and 3) for visual variety. */
const uint8_t world_map[MAP_H][MAP_W] = {
    { 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1 },
    { 1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1 },
    { 1,0,2,2,2,0,0,0,0,0,3,3,3,0,0,1 },
    { 1,0,2,0,0,0,0,0,0,0,3,0,0,0,0,1 },
    { 1,0,2,0,0,0,0,0,0,0,3,0,0,0,0,1 },
    { 1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1 },
    { 1,0,0,0,0,2,2,0,0,0,0,0,0,0,0,1 },
    { 1,0,0,0,0,2,0,0,0,0,0,0,0,0,0,1 },
    { 1,0,0,0,0,2,0,0,0,0,0,0,0,0,0,1 },
    { 1,0,0,0,0,0,0,0,0,3,3,0,0,0,0,1 },
    { 1,0,0,0,0,0,0,0,0,3,0,0,0,0,0,1 },
    { 1,0,0,1,1,1,0,0,0,3,0,0,0,0,0,1 },
    { 1,0,0,1,0,0,0,0,0,0,0,0,0,0,0,1 },
    { 1,0,0,1,0,0,0,0,0,0,2,2,2,0,0,1 },
    { 1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1 },
    { 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1 },
};

const uint8_t wall_colors[3][2] = {
    { 12, 11 }, /* type 1: grey / dark grey (outer wall) */
    { 13,  5 }, /* type 2: light green / green            */
    { 10,  2 }, /* type 3: light red / red                 */
};
