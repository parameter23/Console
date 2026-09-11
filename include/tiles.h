#pragma once
#include <stdint.h>

typedef enum {
    T_EMPTY = 0,
    T_DIRT,
    T_ROCK,
    T_DIAMOND,
    T_STEEL,
    T_EXIT_CLOSED,
    T_EXIT_OPEN,
    T_PLAYER_SPAWN,
    T_EXPLOSION,
    T_MAGIC_WALL,
    T_AMOEBA,
    T_BUTTERFLY_UP,
    T_BUTTERFLY_LEFT,
    T_BUTTERFLY_DOWN,
    T_BUTTERFLY_RIGHT,
    T_MAX
} tile_id_t;

#define MAP_W 20
#define MAP_H 15

extern uint8_t map_data[MAP_H][MAP_W];

uint8_t get_tile(int x, int y);
void set_tile(int x, int y, uint8_t t);
