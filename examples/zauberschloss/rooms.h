/**
 * @file rooms.h
 * @brief The four castle rooms, their persistent tile storage and how
 *        they connect to each other.
 */
#pragma once

#define ROOM_EINGANG       0  /**< Start room; has the castle gate (T_EXIT). */
#define ROOM_WACHRAUM      1  /**< Dead end off Eingang; guard blocks the key. */
#define ROOM_SCHATZKAMMER  2  /**< Off Eingang; has the sword and the locked door. */
#define ROOM_THRONSAAL     3  /**< Behind the locked door; wizard + crown. */
#define ROOM_COUNT         4

/** @brief Movement/room-exit directions, shared with the player's facing. */
#define DIR_UP     0
#define DIR_RIGHT  1
#define DIR_DOWN   2
#define DIR_LEFT   3

/**
 * @brief Builds all four rooms into their persistent tile buffers and
 *        selects ROOM_EINGANG as the active room. Call once at game
 *        start (or on a new game/restart). Also records the wizard's
 *        spawn tile into wizard_spawn_x/y.
 */
void rooms_init(void);

/**
 * @brief Switches get_tile()/set_tile() to the given room's persistent
 *        buffer, without rebuilding it - anything the player changed
 *        (opened doors, picked-up items, a defeated guard) is still there.
 * @param room_id One of the ROOM_* constants.
 */
void room_enter(int room_id);

/**
 * @brief Looks up which room lies in a given direction from a room, if any.
 * @param room_id One of the ROOM_* constants.
 * @param dir     One of the DIR_* constants.
 * @return The neighboring room id, or -1 if that edge has no connection.
 */
int room_neighbor(int room_id, int dir);

/** @brief Tile the wizard starts on in ROOM_THRONSAAL, set by rooms_init(). */
extern int wizard_spawn_x, wizard_spawn_y;
