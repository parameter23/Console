/**
 * @file tiles.h
 * @brief Tile IDs for the scroller-demo. Unlike the room-based examples
 *        (dig-demo, Zauberschloss, Grogs Revenge), there is no
 *        get_tile()/set_tile() grid here - the level is a pixel-scrolled
 *        world described by a per-column height field (see level.h),
 *        and tileset16[] is just the art indexed by these IDs.
 */
#pragma once

typedef enum {
    T_SKY = 0,   /**< Open air above the ground. */
    T_GRASS,     /**< Ground's top row. */
    T_DIRT,      /**< Ground rows below the grass. */
    T_SPIKE,     /**< Obstacle sitting on the ground - touching it costs a life. */
    T_COIN,      /**< Floating collectible. */
    T_GOAL,      /**< The flag at the end of the level. */
    T_MAX
} tile_id_t;
