/**
 * @file tileset16.h
 * @brief Pixel data for the tile_id_t tileset (see tiles.h).
 */
#pragma once
#include "sprite16.h"
#include "tiles.h"

/** @brief One sprite16_t per tile_id_t, indexed by tile_id_t. */
extern const sprite16_t tileset16[T_MAX];
