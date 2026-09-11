/**
 * @file tiles.h
 * @brief Tile map storage and tile IDs for the Zauberschloss RPG.
 *
 * Each room keeps its own persistent tile buffer (see rooms.h); get_tile()/
 * set_tile() always act on whichever room room_enter() last selected, so
 * opened doors, defeated guards and picked-up items stay that way when the
 * player leaves a room and comes back.
 */
#pragma once
#include <stdint.h>

/**
 * @brief Tile IDs used by the castle rooms. T_GUARD is a static
 *        obstacle (defeated in place, never moves); the wizard is a
 *        moving entity and only uses T_WIZARD to mark his spawn point
 *        while a room is being built (see rooms.c) - that tile is
 *        cleared to T_EMPTY once the wizard's start position is read.
 */
typedef enum {
    T_EMPTY = 0,       /**< Floor. */
    T_WALL,            /**< Blocks movement, no action. */
    T_DOOR_LOCKED,     /**< Blocks movement; BTN opens it if the player has the key. */
    T_KEY,             /**< SCHLUESSEL - picked up with BTN. */
    T_SWORD,           /**< SCHWERT - picked up with BTN. */
    T_CROWN,           /**< KRONE - picked up with BTN, alerts the wizard. */
    T_GUARD,           /**< WACHE - blocks movement; BTN fights it (needs sword). */
    T_WIZARD,          /**< ZAUBERER spawn marker, see rooms.c. */
    T_EXIT,            /**< Castle gate; BTN here with the crown wins. */
    T_MAX
} tile_id_t;

#define MAP_W 20
#define MAP_H 14

/**
 * @brief Reads a tile from the currently selected room. Out-of-bounds
 *        coordinates read as T_WALL, so room-edge collision checks
 *        don't need explicit bounds checks.
 * @param x, y Tile coordinates.
 * @return The tile_id_t at (x, y).
 */
uint8_t get_tile(int x, int y);

/**
 * @brief Writes a tile in the currently selected room. Out-of-bounds
 *        coordinates are silently ignored.
 * @param x, y Tile coordinates.
 * @param t    New tile_id_t.
 */
void set_tile(int x, int y, uint8_t t);

/**
 * @brief Points get_tile()/set_tile() at a different room's persistent
 *        tile buffer. Used only by rooms.c (room_enter() during
 *        building, and while constructing each room's initial layout).
 * @param room_map The room's MAP_H x MAP_W buffer.
 */
void tiles_select_map(uint8_t (*room_map)[MAP_W]);
