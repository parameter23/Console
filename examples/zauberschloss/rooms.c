/**
 * @file rooms.c
 * @brief Room layouts, persistent storage and connectivity (see rooms.h).
 *
 * Layout summary (four rooms, castle map):
 *
 *   SCHATZKAMMER --(locked door)--> THRONSAAL
 *         |                              .
 *   EINGANG (start, gate) --> WACHRAUM (guard blocks the key)
 *
 * Required order: get the sword in Schatzkammer, use it on the guard in
 * Wachraum to reach the key, use the key on Schatzkammer's locked door
 * to reach Thronsaal, take the crown (wakes the wizard), escape back to
 * Eingang and use the gate.
 */
#include "rooms.h"
#include "tiles.h"

static uint8_t rooms_data[ROOM_COUNT][MAP_H][MAP_W];

int wizard_spawn_x = 0;
int wizard_spawn_y = 0;

/** @brief Neighbor[room][dir] = target room id, or -1 if that edge is a wall. */
static const int neighbor_table[ROOM_COUNT][4] = {
    /*                    UP  RIGHT  DOWN  LEFT */
    [ROOM_EINGANG]      = { ROOM_SCHATZKAMMER, ROOM_WACHRAUM,     -1, -1 },
    [ROOM_WACHRAUM]     = { -1,                -1,                -1, ROOM_EINGANG },
    [ROOM_SCHATZKAMMER] = { -1,                ROOM_THRONSAAL,    ROOM_EINGANG, -1 },
    [ROOM_THRONSAAL]    = { -1,                -1,                -1, ROOM_SCHATZKAMMER },
};

/** @brief See room_neighbor() in the header for the full contract. */
int room_neighbor(int room_id, int dir)
{
    if (room_id < 0 || room_id >= ROOM_COUNT || dir < 0 || dir > 3)
        return -1;
    return neighbor_table[room_id][dir];
}

/** @brief See room_enter() in the header for the full contract. */
void room_enter(int room_id)
{
    tiles_select_map(rooms_data[room_id]);
}

/**
 * @brief EINGANG: border walls, a 2-tile gap to Schatzkammer (top) and
 *        to Wachraum (right), and the castle gate in the bottom wall.
 */
static void build_eingang(void)
{
    room_enter(ROOM_EINGANG);

    for (int y = 0; y < MAP_H; y++) {
        for (int x = 0; x < MAP_W; x++) {
            uint8_t t = T_EMPTY;

            if (y == 0) {
                t = (x == 9 || x == 10) ? T_EMPTY : T_WALL;
            } else if (y == MAP_H - 1) {
                t = (x == 9 || x == 10) ? T_EXIT : T_WALL;
            } else if (x == 0) {
                t = T_WALL;
            } else if (x == MAP_W - 1) {
                t = (y == 6 || y == 7) ? T_EMPTY : T_WALL;
            }

            set_tile(x, y, t);
        }
    }
}

/**
 * @brief WACHRAUM: dead end off Eingang. An inner wall splits the room
 *        with a single one-tile gap guarded by T_GUARD; the key sits
 *        beyond it.
 */
static void build_wachraum(void)
{
    room_enter(ROOM_WACHRAUM);

    for (int y = 0; y < MAP_H; y++) {
        for (int x = 0; x < MAP_W; x++) {
            uint8_t t = T_EMPTY;

            if (y == 0 || y == MAP_H - 1) {
                t = T_WALL;
            } else if (x == 0) {
                t = (y == 6 || y == 7) ? T_EMPTY : T_WALL;
            } else if (x == MAP_W - 1) {
                t = T_WALL;
            }

            set_tile(x, y, t);
        }
    }

    for (int y = 1; y < MAP_H - 1; y++) {
        if (y != 7)
            set_tile(10, y, T_WALL);
    }
    set_tile(10, 7, T_GUARD);
    set_tile(15, 7, T_KEY);
}

/**
 * @brief SCHATZKAMMER: a 2-tile gap to Eingang (bottom) and a locked
 *        door to Thronsaal (right). Holds the sword.
 */
static void build_schatzkammer(void)
{
    room_enter(ROOM_SCHATZKAMMER);

    for (int y = 0; y < MAP_H; y++) {
        for (int x = 0; x < MAP_W; x++) {
            uint8_t t = T_EMPTY;

            if (y == 0) {
                t = T_WALL;
            } else if (y == MAP_H - 1) {
                t = (x == 9 || x == 10) ? T_EMPTY : T_WALL;
            } else if (x == 0) {
                t = T_WALL;
            } else if (x == MAP_W - 1) {
                t = (y == 6 || y == 7) ? T_DOOR_LOCKED : T_WALL;
            }

            set_tile(x, y, t);
        }
    }

    set_tile(5, 7, T_SWORD);
}

/**
 * @brief THRONSAAL: only connects back to Schatzkammer. Holds the
 *        wizard's spawn marker and the crown.
 */
static void build_thronsaal(void)
{
    room_enter(ROOM_THRONSAAL);

    for (int y = 0; y < MAP_H; y++) {
        for (int x = 0; x < MAP_W; x++) {
            uint8_t t = T_EMPTY;

            if (y == 0 || y == MAP_H - 1) {
                t = T_WALL;
            } else if (x == 0) {
                t = (y == 6 || y == 7) ? T_EMPTY : T_WALL;
            } else if (x == MAP_W - 1) {
                t = T_WALL;
            }

            set_tile(x, y, t);
        }
    }

    wizard_spawn_x = 10;
    wizard_spawn_y = 7;
    set_tile(wizard_spawn_x, wizard_spawn_y, T_EMPTY);
    set_tile(14, 7, T_CROWN);
}

/** @brief See rooms_init() in the header for the full contract. */
void rooms_init(void)
{
    build_eingang();
    build_wachraum();
    build_schatzkammer();
    build_thronsaal();

    room_enter(ROOM_EINGANG);
}
