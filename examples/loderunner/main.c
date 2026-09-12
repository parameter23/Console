/**
 * @file main.c
 * @brief A Lode Runner-style digger-chase game built on the generic
 *        GameAPI: collect all the gold on a level while guards hunt you
 *        down, digging temporary holes in brick tiles to trap them (a
 *        guard - or the runner, if careless - dies if a hole refills
 *        while they're still standing in it) rather than digging
 *        permanent tunnels the way dig-demo's Boulder-Dash-style
 *        digging works. Simplified vs. the 1983 original: no ropes
 *        (ladders alone provide vertical movement), guards use a
 *        greedy chase heuristic (same idea as grogs-revenge's Grog)
 *        instead of real pathfinding, and there's no mid-air steering
 *        while falling.
 *
 * Movement is grid-based like dig-demo/grogs-revenge, not pixel-smooth
 * like scroller-demo. Digging needs two inputs at once (a direction
 * held plus BTN) since the joystick has only one action button, unlike
 * the original's separate dig-left/dig-right keys.
 *
 * Controls: LEFT/RIGHT walk, UP/DOWN climb a ladder you're standing on,
 * BTN+LEFT or BTN+RIGHT digs a hole in the brick tile diagonally below
 * that side.
 *
 * The on-screen font only covers ASCII 32..127 (see font8x8.c), so all
 * German text here is written without umlauts/ß (UE/OE/AE/SS).
 */
#include "game.h"
#include "framebuffer8.h"
#include "sprite16.h"
#include "text.h"
#include "sfx.h"
#include "music.h"
#include "joystick.h"
#include "tiles.h"
#include "tileset16.h"
#include "levels.h"
#include "track.h"

#define TICK_MS 150

#define HOLE_DURATION_TICKS     30  /* ~4.5s before a dug hole refills */
#define GUARD_MOVE_EVERY_N_TICKS 2  /* guards move at half the runner's speed */
#define GUARD_CLIMB_OUT_TICKS    6  /* delay before a trapped guard tries to climb out */
#define GUARD_RESPAWN_TICKS     20  /* delay before a trapped-and-refilled guard returns */
#define GUARD_POOF_TICKS         4  /* how long the death poof is shown */
#define PLAYER_DEATH_TICKS       4  /* poof duration before the level reloads */

#define LIVES_START   3
#define GOLD_SCORE   25
#define GUARD_SCORE  50
#define MAX_HOLES     8

#define MESSAGE_MS 1600

typedef enum {
    STATE_TITEL,
    STATE_SPIEL,
    STATE_LEVEL_CLEAR,
    STATE_SIEG,
    STATE_TOD,
} GameState;

typedef struct {
    int x, y;
    int spawn_x, spawn_y;
    int alive;
    int respawn_ticks;
    int poof_ticks;
    int in_hole_ticks;
    int anim_frame;
} Guard;

typedef struct {
    int x, y;
    int ticks_left;
    int active;
} Hole;

/** @brief 16x16 sprites for the runner/guards/trap-death effect. */
static const sprite16_t runner_sprite_a = {
    .px = {
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,6,6,6,0,0,0,0,0,0},
        {0,0,0,0,0,0,6,6,6,6,6,6,0,0,0,0},
        {0,0,0,0,0,0,6,1,6,1,6,6,0,0,0,0},
        {0,0,0,0,0,0,6,6,6,6,6,6,0,0,0,0},
        {0,0,0,0,0,1,1,1,1,1,0,0,0,0,0,0},
        {0,0,0,0,1,1,1,1,1,1,1,0,0,0,0,0},
        {0,0,0,1,1,1,1,1,1,1,1,1,0,0,0,0},
        {0,0,0,1,1,1,1,1,1,1,1,1,0,0,0,0},
        {0,0,0,0,1,1,1,1,1,1,1,0,0,0,0,0},
        {0,0,0,0,0,1,1,1,1,1,0,0,0,0,0,0},
        {0,0,0,0,0,1,1,1,1,0,0,0,0,0,0,0},
        {0,0,0,1,1,0,0,1,1,0,0,0,0,0,0,0},
        {0,0,0,1,1,0,0,1,1,0,0,0,0,0,0,0},
        {0,0,0,1,1,0,0,1,1,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    }
};

static const sprite16_t runner_sprite_b = {
    .px = {
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,6,6,6,0,0,0,0,0,0},
        {0,0,0,0,0,0,6,6,6,6,6,6,0,0,0,0},
        {0,0,0,0,0,0,6,1,6,1,6,6,0,0,0,0},
        {0,0,0,0,0,0,6,6,6,6,6,6,0,0,0,0},
        {0,0,0,0,0,1,1,1,1,1,0,0,0,0,0,0},
        {0,0,0,0,1,1,1,1,1,1,1,0,0,0,0,0},
        {0,0,0,1,1,1,1,1,1,1,1,1,0,0,0,0},
        {0,0,0,1,1,1,1,1,1,1,1,1,0,0,0,0},
        {0,0,0,0,1,1,1,1,1,1,1,0,0,0,0,0},
        {0,0,0,0,0,1,1,1,1,1,0,0,0,0,0,0},
        {0,0,0,0,1,1,0,1,1,0,0,0,0,0,0,0},
        {0,0,1,1,0,0,0,0,1,1,0,0,0,0,0,0},
        {0,1,1,0,0,0,0,0,0,1,1,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    }
};

static const sprite16_t guard_sprite_a = {
    .px = {
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,2,2,2,0,0,0,0,0,0},
        {0,0,0,0,0,0,2,2,2,2,2,2,0,0,0,0},
        {0,0,0,0,0,0,2,1,2,1,2,2,0,0,0,0},
        {0,0,0,0,0,0,2,2,2,2,2,2,0,0,0,0},
        {0,0,0,0,0,2,2,2,2,2,0,0,0,0,0,0},
        {0,0,0,0,2,2,2,2,2,2,2,0,0,0,0,0},
        {0,0,0,2,2,2,2,2,2,2,2,2,0,0,0,0},
        {0,0,0,2,2,2,2,2,2,2,2,2,0,0,0,0},
        {0,0,0,0,2,2,2,2,2,2,2,0,0,0,0,0},
        {0,0,0,0,0,2,2,2,2,2,0,0,0,0,0,0},
        {0,0,0,0,0,2,2,2,2,0,0,0,0,0,0,0},
        {0,0,0,1,1,0,0,1,1,0,0,0,0,0,0,0},
        {0,0,0,1,1,0,0,1,1,0,0,0,0,0,0,0},
        {0,0,0,1,1,0,0,1,1,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    }
};

static const sprite16_t guard_sprite_b = {
    .px = {
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,2,2,2,0,0,0,0,0,0},
        {0,0,0,0,0,0,2,2,2,2,2,2,0,0,0,0},
        {0,0,0,0,0,0,2,1,2,1,2,2,0,0,0,0},
        {0,0,0,0,0,0,2,2,2,2,2,2,0,0,0,0},
        {0,0,0,0,0,2,2,2,2,2,0,0,0,0,0,0},
        {0,0,0,0,2,2,2,2,2,2,2,0,0,0,0,0},
        {0,0,0,2,2,2,2,2,2,2,2,2,0,0,0,0},
        {0,0,0,2,2,2,2,2,2,2,2,2,0,0,0,0},
        {0,0,0,0,2,2,2,2,2,2,2,0,0,0,0,0},
        {0,0,0,0,0,2,2,2,2,2,0,0,0,0,0,0},
        {0,0,0,0,2,2,0,2,2,0,0,0,0,0,0,0},
        {0,0,2,2,0,0,0,0,2,2,0,0,0,0,0,0},
        {0,2,2,0,0,0,0,0,0,2,2,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    }
};

static const sprite16_t poof_sprite = {
    .px = {
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,7,0,0,0,0,7,0,0,0,0,7,0,0},
        {0,0,0,0,0,7,0,0,0,7,0,0,0,0,0,0},
        {0,0,7,0,0,0,7,0,0,0,7,0,7,0,0,0},
        {0,0,0,0,7,0,0,0,7,0,0,0,7,0,0,0},
        {0,7,0,0,0,7,0,0,0,0,7,0,0,0,7,0},
        {0,0,0,7,0,0,0,0,0,7,0,0,0,0,0,7},
        {0,0,7,0,0,0,0,7,0,0,0,0,7,0,0,0},
        {0,0,0,0,0,7,0,0,0,0,0,7,0,0,0,0},
        {0,7,0,0,0,7,0,0,0,0,0,7,0,0,0,0},
        {0,0,7,0,0,0,7,0,0,0,0,0,7,0,0,0},
        {0,0,0,7,0,0,0,0,7,0,0,0,0,7,0,0},
        {0,0,0,0,7,0,0,0,0,0,7,0,0,0,0,7},
        {0,0,0,0,0,7,0,0,0,7,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    }
};

static GameState state;

static int player_x, player_y;
static int player_facing;      /* -1 left, +1 right - purely cosmetic */
static int player_anim_frame;
static int player_dead_ticks;

static Guard guards[MAX_GUARDS];
static Hole holes[MAX_HOLES];

static int current_level;
static int gold_collected;
static int lives;
static uint32_t score;

static char message[40];
static int32_t message_ms_left;

static int player_died_this_tick;

/** @brief Minimal unsigned-int-to-decimal-string - no sprintf under -nostdlib. */
static void utoa10(uint32_t v, char *out)
{
    char tmp[12];
    int i = 0;
    if (v == 0) {
        out[0] = '0';
        out[1] = 0;
        return;
    }
    while (v > 0 && i < 11) {
        tmp[i++] = (char)('0' + (v % 10));
        v /= 10;
    }
    int j = 0;
    while (i > 0)
        out[j++] = tmp[--i];
    out[j] = 0;
}

/** @brief Appends a NUL-terminated string to dst at *pos, advancing *pos. */
static void append(char *dst, int *pos, const char *s)
{
    while (*s)
        dst[(*pos)++] = *s++;
}

static int iabs(int v) { return v < 0 ? -v : v; }

/** @brief Copies a status message into the message buffer and starts its timer. */
static void say(const char *s)
{
    int i = 0;
    while (s[i] && i < (int)sizeof(message) - 1) {
        message[i] = s[i];
        i++;
    }
    message[i] = 0;
    message_ms_left = MESSAGE_MS;
}

/* ------------------------------------------------------------------- */
/* Movement rules shared by the runner and the guards                   */
/* ------------------------------------------------------------------- */

static int is_solid(uint8_t t)
{
    return t == T_BRICK || t == T_STEEL;
}

static int passable(uint8_t t)
{
    return !is_solid(t);
}

/** @brief True if standing at (x,y) needs no further falling this tick. */
static int has_support(int x, int y)
{
    if (get_tile(x, y) == T_LADDER)
        return 1;
    return is_solid(get_tile(x, y + 1));
}

/* ------------------------------------------------------------------- */
/* Level / entity setup                                                  */
/* ------------------------------------------------------------------- */

static void start_level(int index)
{
    current_level = index;
    level_load(index);

    player_x = level_info.player_start_x;
    player_y = level_info.player_start_y;
    player_facing = 1;
    player_anim_frame = 0;
    player_dead_ticks = 0;

    gold_collected = 0;

    for (int i = 0; i < MAX_HOLES; i++)
        holes[i].active = 0;

    for (int i = 0; i < MAX_GUARDS; i++) {
        if (i < level_info.guard_count) {
            guards[i].spawn_x = level_info.guard_start_x[i];
            guards[i].spawn_y = level_info.guard_start_y[i];
            guards[i].x = guards[i].spawn_x;
            guards[i].y = guards[i].spawn_y;
            guards[i].alive = 1;
        } else {
            guards[i].alive = 0;
        }
        guards[i].respawn_ticks = 0;
        guards[i].poof_ticks = 0;
        guards[i].in_hole_ticks = 0;
        guards[i].anim_frame = 0;
    }
}

static void new_game(void)
{
    lives = LIVES_START;
    score = 0;
    message[0] = 0;
    message_ms_left = 0;
    start_level(0);
}

/** @brief GameAPI init callback. */
static void runner_init(void)
{
    sfx_init();
    music_init(runner_track);
    music_init_bass(runner_bass);
    new_game();
    state = STATE_TITEL;
}

/* ------------------------------------------------------------------- */
/* Digging / holes                                                       */
/* ------------------------------------------------------------------- */

static void try_dig(int x, int y)
{
    if (get_tile(x, y) != T_BRICK)
        return;

    for (int i = 0; i < MAX_HOLES; i++) {
        if (!holes[i].active) {
            holes[i].active = 1;
            holes[i].x = x;
            holes[i].y = y;
            holes[i].ticks_left = HOLE_DURATION_TICKS;
            set_tile(x, y, T_HOLE);
            sfx_play(SFX_NOISE_SHORT);
            return;
        }
    }
}

static void kill_player(void)
{
    if (player_dead_ticks > 0)
        return;
    sfx_play(SFX_EXPLOSION);
    player_dead_ticks = PLAYER_DEATH_TICKS;
    player_died_this_tick = 1;
}

static void kill_guard(int i)
{
    guards[i].alive = 0;
    guards[i].respawn_ticks = GUARD_RESPAWN_TICKS;
    guards[i].poof_ticks = GUARD_POOF_TICKS;
    sfx_play(SFX_EXPLOSION);
    score += GUARD_SCORE;
}

static void holes_update(void)
{
    for (int i = 0; i < MAX_HOLES; i++) {
        if (!holes[i].active)
            continue;

        holes[i].ticks_left--;
        if (holes[i].ticks_left > 0)
            continue;

        if (player_x == holes[i].x && player_y == holes[i].y)
            kill_player();

        for (int g = 0; g < level_info.guard_count; g++) {
            if (guards[g].alive && guards[g].x == holes[i].x && guards[g].y == holes[i].y)
                kill_guard(g);
        }

        set_tile(holes[i].x, holes[i].y, T_BRICK);
        holes[i].active = 0;
    }
}

/* ------------------------------------------------------------------- */
/* Runner                                                                */
/* ------------------------------------------------------------------- */

static void open_exit(void)
{
    for (int y = 0; y < MAP_H; y++)
        for (int x = 0; x < MAP_W; x++)
            if (get_tile(x, y) == T_EXIT_CLOSED)
                set_tile(x, y, T_EXIT_OPEN);
}

static void check_pickup_and_exit(void)
{
    uint8_t t = get_tile(player_x, player_y);
    if (t == T_GOLD) {
        set_tile(player_x, player_y, T_EMPTY);
        gold_collected++;
        score += GOLD_SCORE;
        sfx_play(SFX_PICKUP);
        if (gold_collected >= level_info.gold_total) {
            open_exit();
            say("ALLES GOLD! DER AUSGANG IST OFFEN.");
        }
    } else if (t == T_EXIT_OPEN) {
        sfx_play(SFX_LASER);
        message_ms_left = 0; /* don't let a lingering toast hide the HUD stats */
        if (current_level + 1 >= LEVEL_COUNT) {
            state = STATE_SIEG;
        } else {
            state = STATE_LEVEL_CLEAR;
        }
    }
}

static void player_update(const JoystickState *js)
{
    uint8_t here = get_tile(player_x, player_y);

    if (here == T_HOLE) {
        if ((js->raw & JS_UP) && passable(get_tile(player_x, player_y - 1)))
            player_y--;
        return;
    }

    if (!has_support(player_x, player_y)) {
        player_y++;
        check_pickup_and_exit();
        return;
    }

    if ((js->pressed & JS_BTN) && (js->raw & (JS_LEFT | JS_RIGHT))) {
        int dir = (js->raw & JS_LEFT) ? -1 : 1;
        try_dig(player_x + dir, player_y + 1);
        return;
    }

    int moved = 0;
    if (js->raw & JS_LEFT) {
        if (passable(get_tile(player_x - 1, player_y))) { player_x--; moved = 1; }
        player_facing = -1;
    } else if (js->raw & JS_RIGHT) {
        if (passable(get_tile(player_x + 1, player_y))) { player_x++; moved = 1; }
        player_facing = 1;
    } else if (js->raw & JS_UP) {
        if (here == T_LADDER && passable(get_tile(player_x, player_y - 1))) { player_y--; moved = 1; }
    } else if (js->raw & JS_DOWN) {
        if (here == T_LADDER && passable(get_tile(player_x, player_y + 1))) { player_y++; moved = 1; }
    }

    if (moved) {
        player_anim_frame ^= 1;
        check_pickup_and_exit();
    }
}

/* ------------------------------------------------------------------- */
/* Guards                                                                */
/* ------------------------------------------------------------------- */

static void check_guard_catches_player(const Guard *g)
{
    if (g->alive && g->x == player_x && g->y == player_y)
        kill_player();
}

/** @brief Greedy chase toward the runner - same idea as grogs-revenge's
 *         Grog, with a random-wander fallback so a blocked guard doesn't
 *         freeze forever. Not real pathfinding. */
static void guard_chase(Guard *g)
{
    int dx = player_x - g->x, dy = player_y - g->y;
    int sx = (dx > 0) - (dx < 0), sy = (dy > 0) - (dy < 0);
    int ax = iabs(dx), ay = iabs(dy);
    uint8_t here = get_tile(g->x, g->y);
    int moved = 0;

    if (ax >= ay) {
        if (sx != 0 && passable(get_tile(g->x + sx, g->y))) { g->x += sx; moved = 1; }
        else if (sy != 0 && here == T_LADDER && passable(get_tile(g->x, g->y + sy))) { g->y += sy; moved = 1; }
    } else {
        if (sy != 0 && here == T_LADDER && passable(get_tile(g->x, g->y + sy))) { g->y += sy; moved = 1; }
        else if (sx != 0 && passable(get_tile(g->x + sx, g->y))) { g->x += sx; moved = 1; }
    }

    if (!moved) {
        if (passable(get_tile(g->x - 1, g->y))) { g->x--; moved = 1; }
        else if (passable(get_tile(g->x + 1, g->y))) { g->x++; moved = 1; }
        else if (here == T_LADDER && passable(get_tile(g->x, g->y - 1))) { g->y--; moved = 1; }
        else if (here == T_LADDER && passable(get_tile(g->x, g->y + 1))) { g->y++; moved = 1; }
    }

    if (moved)
        g->anim_frame ^= 1;
}

static uint32_t guard_move_counter;

static void guards_update(void)
{
    guard_move_counter++;
    int should_move = (guard_move_counter % GUARD_MOVE_EVERY_N_TICKS) == 0;

    for (int i = 0; i < level_info.guard_count; i++) {
        Guard *g = &guards[i];

        if (!g->alive) {
            if (g->poof_ticks > 0)
                g->poof_ticks--;
            if (g->respawn_ticks > 0) {
                g->respawn_ticks--;
                if (g->respawn_ticks == 0) {
                    g->x = g->spawn_x;
                    g->y = g->spawn_y;
                    g->alive = 1;
                    g->in_hole_ticks = 0;
                }
            }
            continue;
        }

        uint8_t here = get_tile(g->x, g->y);

        if (here == T_HOLE) {
            g->in_hole_ticks++;
            if (g->in_hole_ticks >= GUARD_CLIMB_OUT_TICKS && passable(get_tile(g->x, g->y - 1))) {
                g->y--;
                g->in_hole_ticks = 0;
            }
            continue;
        }
        g->in_hole_ticks = 0;

        if (!has_support(g->x, g->y)) {
            g->y++;
            check_guard_catches_player(g);
            if (player_died_this_tick) return;
            continue;
        }

        if (!should_move)
            continue;

        guard_chase(g);
        check_guard_catches_player(g);
        if (player_died_this_tick) return;
    }
}

/* ------------------------------------------------------------------- */
/* GameAPI update                                                        */
/* ------------------------------------------------------------------- */

static void spiel_tick(const JoystickState *js)
{
    if (player_dead_ticks > 0) {
        player_dead_ticks--;
        if (player_dead_ticks == 0) {
            lives--;
            if (lives <= 0)
                state = STATE_TOD;
            else
                start_level(current_level);
        }
        return;
    }

    player_died_this_tick = 0;

    player_update(js);
    if (player_died_this_tick || state != STATE_SPIEL)
        return;

    holes_update();
    if (player_died_this_tick || state != STATE_SPIEL)
        return;

    guards_update();
}

/** @brief GameAPI update callback. */
static void runner_update(const JoystickState *js, uint32_t tick_ms)
{
    if (message_ms_left > 0) {
        message_ms_left -= (int32_t)tick_ms;
        if (message_ms_left < 0)
            message_ms_left = 0;
    }

    switch (state) {
    case STATE_TITEL:
        if (js->pressed & JS_BTN) {
            new_game();
            state = STATE_SPIEL;
        }
        break;

    case STATE_SPIEL:
        spiel_tick(js);
        break;

    case STATE_LEVEL_CLEAR:
        if (js->pressed & JS_BTN) {
            start_level(current_level + 1);
            state = STATE_SPIEL;
        }
        break;

    case STATE_SIEG:
    case STATE_TOD:
        if (js->pressed & JS_BTN) {
            new_game();
            state = STATE_SPIEL;
        }
        break;
    }
}

/* ------------------------------------------------------------------- */
/* Rendering                                                             */
/* ------------------------------------------------------------------- */

static void draw_center(int y, const char *s, uint8_t color)
{
    int len = 0;
    while (s[len]) len++;
    int x = (FB8_WIDTH - len * 8) / 2;
    if (x < 0) x = 0;
    draw_text(x, y, s, color);
}

static void draw_titel(void)
{
    draw_center(30,  "GRABENLAEUFER", 5);
    draw_center(56,  "EIN LODE-RUNNER-ARTIGES ABENTEUER", 15);
    draw_center(90,  "SAMMLE ALLES GOLD UND ERREICHE", 1);
    draw_center(104, "DEN AUSGANG - MEIDE DIE WAECHTER.", 1);
    draw_center(138, "PFEILE: LAUFEN/KLETTERN AUF LEITERN", 2);
    draw_center(152, "KNOPF+PFEIL: LOCH GRABEN", 2);
    draw_center(166, "GRAB EINEM WAECHTER EIN LOCH VOR DIE", 2);
    draw_center(180, "FUESSE, DAMIT ER DARIN VERSCHWINDET!", 2);
    draw_center(214, "DRUECKE DEN KNOPF", 13);
}

static void draw_hud(void)
{
    char line[64];
    int p = 0;
    char num[12];

    if (message_ms_left > 0) {
        draw_text(4, FB8_HEIGHT - 10, message, 7);
        return;
    }

    append(line, &p, "PUNKTE ");
    utoa10(score, num);
    append(line, &p, num);

    append(line, &p, " GOLD ");
    utoa10((uint32_t)gold_collected, num);
    append(line, &p, num);
    append(line, &p, "/");
    utoa10((uint32_t)level_info.gold_total, num);
    append(line, &p, num);

    append(line, &p, " LEBEN ");
    utoa10((uint32_t)(lives < 0 ? 0 : lives), num);
    append(line, &p, num);

    append(line, &p, " LV ");
    utoa10((uint32_t)(current_level + 1), num);
    append(line, &p, num);
    append(line, &p, "/");
    utoa10((uint32_t)LEVEL_COUNT, num);
    append(line, &p, num);

    line[p] = 0;
    draw_text(4, FB8_HEIGHT - 10, line, 1);
}

static void draw_ende(int sieg)
{
    if (sieg) {
        draw_center(90,  "ALLE LEVEL GESCHAFFT!", 13);
        draw_center(110, "DU BIST DER MEISTER-GRABENLAEUFER.", 1);
    } else {
        draw_center(90,  "GAME OVER", 2);
        draw_center(110, "DIE WAECHTER HABEN DICH ERWISCHT.", 1);
    }
    draw_hud();
    draw_center(150, "DRUECKE DEN KNOPF", 15);
}

static void draw_level_clear(void)
{
    draw_center(100, "LEVEL GESCHAFFT!", 13);
    draw_hud();
    draw_center(150, "DRUECKE KNOPF - NAECHSTES LEVEL", 15);
}

static void draw_map(void)
{
    for (int y = 0; y < MAP_H; y++)
        for (int x = 0; x < MAP_W; x++)
            draw_sprite16(x * 16, y * 16, &tileset16[get_tile(x, y)]);
}

static void draw_guards(void)
{
    for (int i = 0; i < level_info.guard_count; i++) {
        const Guard *g = &guards[i];
        if (g->alive) {
            draw_sprite16(g->x * 16, g->y * 16, g->anim_frame ? &guard_sprite_b : &guard_sprite_a);
        } else if (g->poof_ticks > 0) {
            draw_sprite16(g->x * 16, g->y * 16, &poof_sprite);
        }
    }
}

static void draw_player(void)
{
    if (player_dead_ticks > 0)
        draw_sprite16(player_x * 16, player_y * 16, &poof_sprite);
    else
        draw_sprite16(player_x * 16, player_y * 16, player_anim_frame ? &runner_sprite_b : &runner_sprite_a);
}

/** @brief GameAPI draw callback. */
static void runner_draw(void)
{
    if (state == STATE_TITEL) {
        draw_titel();
        return;
    }

    draw_map();
    draw_guards();
    draw_player();

    switch (state) {
    case STATE_SIEG:
        draw_ende(1);
        break;
    case STATE_TOD:
        draw_ende(0);
        break;
    case STATE_LEVEL_CLEAR:
        draw_level_clear();
        break;
    default:
        draw_hud();
        break;
    }
}

static const GameAPI loderunner_game = {
    .name   = "loderunner",
    .init   = runner_init,
    .update = runner_update,
    .draw   = runner_draw,
};

/** @brief Entry point: runs the game via the generic GameAPI loop. */
int main(void)
{
    game_run(&loderunner_game, TICK_MS);
    return 0;
}
