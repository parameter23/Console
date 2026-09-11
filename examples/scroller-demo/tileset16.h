/**
 * @file tileset16.h
 * @brief 16x16 sprite art for each tile_id_t (see tiles.h).
 */
#ifndef GAME_TILESET16_H
#define GAME_TILESET16_H

#include "sprite16.h"
#include "tiles.h"   /* T_SKY ... T_MAX */

/** @brief One sprite16_t per tile_id_t, indexed by tile ID. */
extern const sprite16_t tileset16[T_MAX];

#endif
