/**
 * @file main.c
 * @brief "Invaders" - a Space-Invaders-style shooter built on the generic
 *        GameAPI. A rigid grid of aliens marches left/right and steps down
 *        each time it hits the screen edge, picking up speed as fewer of
 *        them remain, exactly like the 1978 arcade original. Each alien
 *        type has a two-frame walk-cycle animation that alternates on
 *        every formation step, four destructible shield bunkers sit
 *        between the player and the aliens, and a bonus UFO occasionally
 *        crosses the top of the screen.
 *
 * Controls: the analog stick's X axis steers the cannon (proportional
 * speed, deadzone near center - see analog_move_delta()), BTN fires (one
 * shot in flight at a time, as in the original). Survive as many waves
 * as possible; the game ends when you run out of lives or the aliens
 * reach the shields.
 *
 * The on-screen font (see font8x8.c) covers full Code Page 850, and
 * draw_text() decodes UTF-8, so German umlauts/ß can be written directly.
 */
#include "game.h"
#include "framebuffer8.h"
#include "sprite16.h"
#include "text.h"
#include "sfx.h"
#include "joystick.h"

#define TICK_MS  30   /* short tick -> smooth cannon/bullet motion */
#define MESSAGE_MS  1500

/* --- Alien formation layout --- */
#define ALIEN_COLS       8
#define ALIEN_ROWS       4
#define ALIEN_SPACING_X  24
#define ALIEN_SPACING_Y  20
#define ALIEN_START_X    24
#define ALIEN_START_Y    28
#define ALIEN_STEP_PX    6
#define ALIEN_DROP_PX    8
#define ALIEN_EDGE_MARGIN 8
#define ALIEN_EXPLODE_TICKS 6

/* --- Player --- */
#define PLAYER_Y            (FB8_HEIGHT - 26)
#define PLAYER_LIVES_START  3
#define PLAYER_EXPLODE_TICKS 15

/* Analog stick: PA0/ADC1_IN0 rests near mid-scale (2048 of 0..4095) on a
 * standard joystick module; JS_X_DEADZONE absorbs resting jitter, and the
 * remaining travel is mapped linearly to a steering speed of up to
 * PLAYER_MAX_SPEED px/tick - full deflection = full speed, proportional
 * in between, instead of the on/off digital-button motion. */
#define JS_X_CENTER      2048
#define JS_X_DEADZONE    300
#define PLAYER_MAX_SPEED 6

/* --- Bullets --- */
#define PLAYER_BULLET_SPEED  10
#define ALIEN_BULLET_SPEED   5
#define ALIEN_BULLETS_MAX    3

/* --- Shields (destructible bunkers) --- */
#define SHIELD_COUNT  4
#define SHIELD_COLS   8
#define SHIELD_ROWS   6
#define SHIELD_BLOCK  4
#define SHIELD_Y      (PLAYER_Y - 50)

/* --- Bonus UFO --- */
#define UFO_Y            10
#define UFO_SPEED         3
#define UFO_SPAWN_MIN_MS  7000
#define UFO_SPAWN_MAX_MS  14000

typedef enum {
    STATE_TITEL,
    STATE_SPIEL,
    STATE_TOD,
} GameState;

/* ------------------------------------------------------------------- */
/* Sprites (16x16 paletted, index 0 = transparent). Each alien type has */
/* two animation frames that alternate on every formation step.        */
/* ------------------------------------------------------------------- */

/** @brief Top-row alien ('squid'), frame A, 30 pts. */
static const sprite16_t alien_squid_a = {
    .px = {
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,3,3,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,3,3,3,3,0,0,0,0,0,0},
        {0,0,0,0,0,3,3,3,3,3,3,0,0,0,0,0},
        {0,0,0,0,3,3,0,3,3,0,3,3,0,0,0,0},
        {0,0,0,3,3,3,3,3,3,3,3,3,3,0,0,0},
        {0,0,3,3,0,3,3,3,3,3,3,0,3,3,0,0},
        {0,0,3,3,0,3,3,3,3,3,3,0,3,3,0,0},
        {0,0,0,3,3,3,3,3,3,3,3,3,3,0,0,0},
        {0,0,0,0,0,3,0,0,0,0,3,0,0,0,0,0},
        {0,0,0,0,3,3,0,0,0,0,3,3,0,0,0,0},
        {0,0,0,3,3,0,0,0,0,0,0,3,3,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    }
};

/** @brief Top-row alien ('squid'), frame B. */
static const sprite16_t alien_squid_b = {
    .px = {
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,3,3,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,3,3,3,3,0,0,0,0,0,0},
        {0,0,0,0,0,3,3,3,3,3,3,0,0,0,0,0},
        {0,0,0,0,3,3,0,3,3,0,3,3,0,0,0,0},
        {0,0,0,3,3,3,3,3,3,3,3,3,3,0,0,0},
        {0,0,3,3,0,3,3,3,3,3,3,0,3,3,0,0},
        {0,0,3,3,0,3,3,3,3,3,3,0,3,3,0,0},
        {0,0,0,3,3,3,3,3,3,3,3,3,3,0,0,0},
        {0,0,0,0,3,3,0,0,0,0,3,3,0,0,0,0},
        {0,0,0,3,3,0,0,3,3,0,0,3,3,0,0,0},
        {0,0,3,3,0,0,0,0,3,3,0,0,3,3,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    }
};

/** @brief Mid-row alien ('crab'), frame A, 20 pts. */
static const sprite16_t alien_crab_a = {
    .px = {
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,4,0,0,0,0,0,0,0,0,0,0,0,0,4},
        {0,0,0,4,0,0,0,0,0,0,0,0,0,0,4,0},
        {0,0,4,4,4,0,0,0,0,0,0,0,0,4,4,4},
        {0,4,4,0,4,4,4,4,4,4,4,4,0,4,4,0},
        {4,4,0,4,4,4,4,4,4,4,4,4,4,0,4,4},
        {4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4},
        {4,4,0,4,4,0,4,4,4,4,0,4,4,0,4,4},
        {4,4,4,4,4,4,0,0,0,0,4,4,4,4,4,4},
        {0,0,0,4,4,0,0,0,0,0,0,4,4,0,0,0},
        {0,0,4,4,0,0,4,4,4,4,0,0,4,4,0,0},
        {0,4,4,0,0,0,0,4,4,0,0,0,0,4,4,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    }
};

/** @brief Mid-row alien ('crab'), frame B. */
static const sprite16_t alien_crab_b = {
    .px = {
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,4,0,0,0,0,0,0,0,0,4,0,0},
        {0,4,0,0,0,4,0,0,0,0,0,0,4,0,0,0},
        {0,4,4,4,0,0,0,0,0,0,0,0,4,4,4,0},
        {0,4,4,0,4,4,4,4,4,4,4,4,0,4,4,0},
        {4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4},
        {4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4},
        {4,4,0,4,4,0,4,4,4,4,0,4,4,0,4,4},
        {4,4,4,4,4,4,0,0,0,0,4,4,4,4,4,4},
        {0,0,4,4,0,0,0,0,0,0,0,0,4,4,0,0},
        {0,4,4,0,0,0,0,0,0,0,0,0,0,4,4,0},
        {4,0,0,0,0,0,0,0,0,0,0,0,0,0,0,4},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    }
};

/** @brief Bottom-row alien ('octopus'), frame A, 10 pts. */
static const sprite16_t alien_octo_a = {
    .px = {
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,5,0,0,0,0,0,0,5,0,0,0,0},
        {0,0,0,0,0,5,0,0,0,0,5,0,0,0,0,0},
        {0,0,0,0,5,5,5,5,5,5,5,5,0,0,0,0},
        {0,0,0,5,5,0,5,5,5,5,0,5,5,0,0,0},
        {0,0,5,5,5,5,5,5,5,5,5,5,5,5,0,0},
        {0,0,5,5,0,5,5,5,5,5,5,0,5,5,0,0},
        {0,0,5,5,5,5,5,5,5,5,5,5,5,5,0,0},
        {0,0,0,0,5,5,0,0,0,0,5,5,0,0,0,0},
        {0,0,0,5,5,0,0,5,5,0,0,5,5,0,0,0},
        {0,0,5,5,0,0,5,5,0,0,5,5,0,0,5,5},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    }
};

/** @brief Bottom-row alien ('octopus'), frame B. */
static const sprite16_t alien_octo_b = {
    .px = {
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,5,0,0,0,0,0,0,5,0,0,0,0},
        {0,0,0,0,0,5,0,0,0,0,5,0,0,0,0,0},
        {0,0,0,0,5,5,5,5,5,5,5,5,0,0,0,0},
        {0,0,0,5,5,0,5,5,5,5,0,5,5,0,0,0},
        {0,0,5,5,5,5,5,5,5,5,5,5,5,5,0,0},
        {0,0,5,5,0,5,5,5,5,5,5,0,5,5,0,0},
        {0,0,5,5,5,5,5,5,5,5,5,5,5,5,0,0},
        {0,0,0,5,5,0,5,5,0,5,5,0,0,0,0,0},
        {0,0,5,5,0,0,5,5,0,0,5,5,0,0,0,0},
        {0,5,5,0,0,0,0,5,5,0,0,0,0,5,5,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    }
};

/** @brief Player cannon. */
static const sprite16_t player_sprite = {
    .px = {
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,1,1,1,1,0,0,0,0,0,0},
        {0,0,0,0,0,0,1,1,1,1,0,0,0,0,0,0},
        {0,0,0,0,0,1,1,1,1,1,1,0,0,0,0,0},
        {0,0,0,0,0,1,1,1,1,1,1,0,0,0,0,0},
        {0,0,0,0,1,1,1,1,1,1,1,1,0,0,0,0},
        {0,0,0,0,1,1,0,2,2,0,1,1,0,0,0,0},
        {0,0,0,1,1,1,1,1,1,1,1,1,1,0,0,0},
        {0,0,0,1,1,1,1,1,1,1,1,1,1,0,0,0},
        {0,0,1,1,1,1,1,1,1,1,1,1,1,1,0,0},
        {0,0,1,1,1,1,1,1,1,1,1,1,1,1,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    }
};

/** @brief Bonus UFO ('mystery ship'). */
static const sprite16_t ufo_sprite = {
    .px = {
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,8,8,8,8,0,0,0,0,0,0},
        {0,0,0,0,8,8,8,8,8,8,8,8,8,0,0,0},
        {0,0,0,8,8,8,8,8,8,8,8,8,8,8,0,0},
        {0,0,8,8,8,8,8,8,8,8,8,8,8,8,8,0},
        {0,8,7,7,8,7,7,8,7,7,8,7,7,8,8,0},
        {8,8,8,8,8,8,8,8,8,8,8,8,8,8,0,0},
        {0,0,8,8,8,8,8,8,8,8,8,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    }
};

/** @brief Shared hit/death explosion burst. */
static const sprite16_t explosion_sprite = {
    .px = {
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,8,0,0,0,8,0,0,0,0,8,0,0,0,0},
        {0,0,0,2,0,0,0,0,0,2,0,0,0,0,0,0},
        {0,8,0,0,0,0,7,0,0,0,0,0,0,8,0,0},
        {0,0,0,0,2,0,0,0,0,2,0,0,0,0,0,0},
        {0,0,7,0,0,0,0,8,0,0,0,7,0,0,0,0},
        {0,0,0,0,0,0,0,2,0,0,0,0,0,0,0,0},
        {0,0,8,0,0,2,0,0,0,0,2,0,0,8,0,0},
        {0,0,0,0,7,0,0,0,0,0,0,0,7,0,0,0},
        {0,8,0,0,0,0,0,2,0,0,0,0,8,0,0,0},
        {0,0,2,0,0,0,7,0,0,0,0,0,0,2,0,0},
        {0,0,0,0,8,0,0,0,0,0,0,7,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    }
};

/* ------------------------------------------------------------------- */
/* Game state                                                           */
/* ------------------------------------------------------------------- */

typedef struct {
    uint8_t alive;
    uint8_t explode_ticks; /* >0 while the death burst is showing */
} AlienCell;

typedef struct {
    int x, y;
    int active;
} Bullet;

static GameState state;

static AlienCell aliens[ALIEN_ROWS][ALIEN_COLS];
static int alien_origin_x, alien_origin_y;
static int alien_dir;          /* +1 = right, -1 = left */
static int alien_anim_frame;   /* 0/1, toggled every formation step */
static uint32_t alien_move_ticks_left;

static Bullet player_bullet;
static Bullet alien_bullets[ALIEN_BULLETS_MAX];

static int player_x;
static int player_lives;
static int player_explode_ticks;

static int ufo_active;
static int ufo_x;
static int ufo_dir;
static uint32_t ufo_spawn_ms_left;
static int ufo_score_value;

static uint32_t score;
static int wave;

static uint8_t shield_cell[SHIELD_COUNT][SHIELD_ROWS][SHIELD_COLS];
static const int shield_x[SHIELD_COUNT] = { 38, 108, 178, 248 };

static char message[40];
static int32_t message_ms_left;

static uint32_t rng_state = 0xC0FFEEu;

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

/** @brief Tiny LCG PRNG - good enough for shot timing and UFO bonuses. */
static uint32_t rng_next(void)
{
    rng_state = rng_state * 1664525u + 1013904223u;
    return rng_state;
}

/* ------------------------------------------------------------------- */
/* Animated starfield background - a simple 3-layer parallax drift,    */
/* drawn behind everything (including the title/game-over screens).   */
/* ------------------------------------------------------------------- */

#define STAR_COUNT 48

typedef struct {
    int16_t x, y;
    uint8_t layer; /* 0 = far/dim/slow .. 2 = near/bright/fast */
} Star;

static Star stars[STAR_COUNT];

/** @brief Scatters all stars at random positions/depths. Call once. */
static void init_stars(void)
{
    for (int i = 0; i < STAR_COUNT; i++) {
        stars[i].x = (int16_t)(rng_next() % FB8_WIDTH);
        stars[i].y = (int16_t)(rng_next() % FB8_HEIGHT);
        stars[i].layer = (uint8_t)(rng_next() % 3);
    }
}

/** @brief Drifts every star downward, faster/brighter layers moving more,
 *         and recycles it to the top with a fresh x once it exits the
 *         bottom of the screen - a cheap infinite parallax scroll. */
static void update_stars(void)
{
    for (int i = 0; i < STAR_COUNT; i++) {
        stars[i].y += (int16_t)(stars[i].layer + 1);
        if (stars[i].y >= FB8_HEIGHT) {
            stars[i].y = 0;
            stars[i].x = (int16_t)(rng_next() % FB8_WIDTH);
        }
    }
}

static void draw_stars(void)
{
    static const uint8_t layer_color[3] = { 11, 12, 15 }; /* dark/mid/light grey */
    for (int i = 0; i < STAR_COUNT; i++) {
        if (stars[i].layer == 2)
            fb8_fill_rect(stars[i].x, stars[i].y, 2, 2, layer_color[2]);
        else
            fb8_set_pixel(stars[i].x, stars[i].y, layer_color[stars[i].layer]);
    }
}

static int rects_overlap(int ax, int ay, int aw, int ah,
                          int bx, int by, int bw, int bh)
{
    return ax < bx + bw && ax + aw > bx &&
           ay < by + bh && ay + ah > by;
}

/** @brief Top-left pixel position of alien grid cell (r, c) right now. */
static void alien_pixel_pos(int r, int c, int *x, int *y)
{
    *x = alien_origin_x + c * ALIEN_SPACING_X;
    *y = alien_origin_y + r * ALIEN_SPACING_Y;
}

static int alien_alive_count(void)
{
    int n = 0;
    for (int r = 0; r < ALIEN_ROWS; r++)
        for (int c = 0; c < ALIEN_COLS; c++)
            if (aliens[r][c].alive)
                n++;
    return n;
}

/** @brief Lowest (frontmost) alive row in column c, or -1 if empty. */
static int alien_bottom_row(int c)
{
    for (int r = ALIEN_ROWS - 1; r >= 0; r--)
        if (aliens[r][c].alive)
            return r;
    return -1;
}

static void kill_alien(int r, int c)
{
    aliens[r][c].alive = 0;
    aliens[r][c].explode_ticks = ALIEN_EXPLODE_TICKS;

    uint32_t pts = (r == 0) ? 30 : (r == ALIEN_ROWS - 1) ? 10 : 20;
    score += pts;
    sfx_play(SFX_EXPLOSION);
}

/** @brief Carves a rough arch shape into one shield's block grid. */
static void build_shield(int s)
{
    for (int r = 0; r < SHIELD_ROWS; r++) {
        for (int c = 0; c < SHIELD_COLS; c++) {
            int alive = 1;
            if (r == 0 && (c == 0 || c == SHIELD_COLS - 1))
                alive = 0; /* rounded top corners */
            if (r >= SHIELD_ROWS - 2 && (c == SHIELD_COLS / 2 - 1 || c == SHIELD_COLS / 2))
                alive = 0; /* bottom entry notch */
            shield_cell[s][r][c] = (uint8_t)alive;
        }
    }
}

/**
 * @brief Tests a bullet's rectangle against every shield; destroys the
 *        first alive block it overlaps and reports a hit.
 * @return 1 if a block was destroyed (bullet should stop), else 0.
 */
static int shield_hit_test(int x, int y, int w, int h)
{
    for (int s = 0; s < SHIELD_COUNT; s++) {
        int sx = shield_x[s], sy = SHIELD_Y;
        int sw = SHIELD_COLS * SHIELD_BLOCK, sh = SHIELD_ROWS * SHIELD_BLOCK;
        if (!rects_overlap(x, y, w, h, sx, sy, sw, sh))
            continue;

        int c0 = (x - sx) / SHIELD_BLOCK;
        int c1 = (x + w - 1 - sx) / SHIELD_BLOCK;
        int r0 = (y - sy) / SHIELD_BLOCK;
        int r1 = (y + h - 1 - sy) / SHIELD_BLOCK;
        if (c0 < 0) c0 = 0;
        if (r0 < 0) r0 = 0;
        if (c1 >= SHIELD_COLS) c1 = SHIELD_COLS - 1;
        if (r1 >= SHIELD_ROWS) r1 = SHIELD_ROWS - 1;

        for (int r = r0; r <= r1; r++) {
            for (int c = c0; c <= c1; c++) {
                if (shield_cell[s][r][c]) {
                    shield_cell[s][r][c] = 0;
                    return 1;
                }
            }
        }
    }
    return 0;
}

/** @brief Resets/advances the alien grid for a new wave. */
static void start_wave(void)
{
    for (int r = 0; r < ALIEN_ROWS; r++)
        for (int c = 0; c < ALIEN_COLS; c++) {
            aliens[r][c].alive = 1;
            aliens[r][c].explode_ticks = 0;
        }

    alien_origin_x = ALIEN_START_X;
    alien_origin_y = ALIEN_START_Y + (wave - 1) * 6;
    if (alien_origin_y > ALIEN_START_Y + 36)
        alien_origin_y = ALIEN_START_Y + 36;
    alien_dir = 1;
    alien_anim_frame = 0;
    alien_move_ticks_left = 1;

    for (int i = 0; i < ALIEN_BULLETS_MAX; i++)
        alien_bullets[i].active = 0;
    player_bullet.active = 0;

    ufo_active = 0;
    ufo_spawn_ms_left = UFO_SPAWN_MIN_MS + (rng_next() % (UFO_SPAWN_MAX_MS - UFO_SPAWN_MIN_MS));
}

/** @brief Starts a fresh game: score/lives/wave reset, first wave built. */
static void neues_spiel(void)
{
    score = 0;
    wave = 1;
    player_lives = PLAYER_LIVES_START;
    player_x = (FB8_WIDTH - 16) / 2;
    player_explode_ticks = 0;
    message[0] = 0;
    message_ms_left = 0;

    for (int s = 0; s < SHIELD_COUNT; s++)
        build_shield(s);

    start_wave();
}

/** @brief GameAPI init callback. */
static void invaders_init(void)
{
    sfx_init();
    rng_state ^= ((uint32_t)joystick_get_adc_x() << 16) ^ joystick_get_adc_y();
    init_stars();
    neues_spiel();
    state = STATE_TITEL;
}

/** @brief Moves/advances the alien formation by one step, or drops+flips. */
static void step_formation(void)
{
    int min_col = ALIEN_COLS, max_col = -1, min_row = ALIEN_ROWS, max_row = -1;
    for (int r = 0; r < ALIEN_ROWS; r++) {
        for (int c = 0; c < ALIEN_COLS; c++) {
            if (!aliens[r][c].alive)
                continue;
            if (c < min_col) min_col = c;
            if (c > max_col) max_col = c;
            if (r < min_row) min_row = r;
            if (r > max_row) max_row = r;
        }
    }
    if (max_col < 0)
        return; /* wave already cleared */

    int next_origin_x = alien_origin_x + alien_dir * ALIEN_STEP_PX;
    int next_left = next_origin_x + min_col * ALIEN_SPACING_X;
    int next_right = next_origin_x + max_col * ALIEN_SPACING_X + 16;

    if (next_left < ALIEN_EDGE_MARGIN || next_right > FB8_WIDTH - ALIEN_EDGE_MARGIN) {
        alien_dir = -alien_dir;
        alien_origin_y += ALIEN_DROP_PX;
    } else {
        alien_origin_x = next_origin_x;
    }
    alien_anim_frame ^= 1;
    sfx_play(SFX_MOVE);

    if (alien_origin_y + max_row * ALIEN_SPACING_Y + 16 >= PLAYER_Y - 4) {
        state = STATE_TOD;
        say("DIE ALIENS SIND GELANDET!");
        sfx_play(SFX_SIREN);
    }
}

static int count_active_alien_bullets(void)
{
    int n = 0;
    for (int i = 0; i < ALIEN_BULLETS_MAX; i++)
        if (alien_bullets[i].active)
            n++;
    return n;
}

/** @brief Occasionally has a random front-line alien fire downward. */
static void alien_try_shoot(void)
{
    if (count_active_alien_bullets() >= ALIEN_BULLETS_MAX)
        return;

    uint32_t threshold = 8 + (uint32_t)wave * 2;
    if (rng_next() % 1000 >= threshold)
        return;

    int cols[ALIEN_COLS];
    int n = 0;
    for (int c = 0; c < ALIEN_COLS; c++)
        if (alien_bottom_row(c) >= 0)
            cols[n++] = c;
    if (n == 0)
        return;

    int c = cols[rng_next() % (uint32_t)n];
    int r = alien_bottom_row(c);
    int ax, ay;
    alien_pixel_pos(r, c, &ax, &ay);

    for (int i = 0; i < ALIEN_BULLETS_MAX; i++) {
        if (!alien_bullets[i].active) {
            alien_bullets[i].active = 1;
            alien_bullets[i].x = ax + 7;
            alien_bullets[i].y = ay + 16;
            break;
        }
    }
}

/** @brief Spawns the bonus UFO from a random side when its timer expires. */
static void ufo_update(uint32_t tick_ms)
{
    if (!ufo_active) {
        if (ufo_spawn_ms_left > tick_ms) {
            ufo_spawn_ms_left -= tick_ms;
            return;
        }
        ufo_active = 1;
        ufo_dir = (rng_next() & 1) ? 1 : -1;
        ufo_x = (ufo_dir > 0) ? -16 : FB8_WIDTH;
        static const int values[4] = { 50, 100, 150, 300 };
        ufo_score_value = values[rng_next() % 4];
        return;
    }

    ufo_x += ufo_dir * UFO_SPEED;
    if (ufo_x < -20 || ufo_x > FB8_WIDTH + 20) {
        ufo_active = 0;
        ufo_spawn_ms_left = UFO_SPAWN_MIN_MS + (rng_next() % (UFO_SPAWN_MAX_MS - UFO_SPAWN_MIN_MS));
    }
}

/** @brief Advances the player's bullet, checking shields/aliens/UFO. */
static void player_bullet_update(void)
{
    if (!player_bullet.active)
        return;

    player_bullet.y -= PLAYER_BULLET_SPEED;
    if (player_bullet.y < 0) {
        player_bullet.active = 0;
        return;
    }

    if (shield_hit_test(player_bullet.x, player_bullet.y, 2, 8)) {
        player_bullet.active = 0;
        return;
    }

    for (int r = 0; r < ALIEN_ROWS; r++) {
        for (int c = 0; c < ALIEN_COLS; c++) {
            if (!aliens[r][c].alive)
                continue;
            int ax, ay;
            alien_pixel_pos(r, c, &ax, &ay);
            if (rects_overlap(player_bullet.x, player_bullet.y, 2, 8, ax, ay, 16, 16)) {
                kill_alien(r, c);
                player_bullet.active = 0;
                return;
            }
        }
    }

    if (ufo_active &&
        rects_overlap(player_bullet.x, player_bullet.y, 2, 8, ufo_x, UFO_Y, 16, 16)) {
        score += (uint32_t)ufo_score_value;
        ufo_active = 0;
        ufo_spawn_ms_left = UFO_SPAWN_MIN_MS + (rng_next() % (UFO_SPAWN_MAX_MS - UFO_SPAWN_MIN_MS));
        player_bullet.active = 0;
        sfx_play(SFX_EXPLOSION);
    }
}

/** @brief Costs a life and either ends the game or respawns the cannon. */
static void player_hit(void)
{
    sfx_play(SFX_EXPLOSION);
    player_explode_ticks = PLAYER_EXPLODE_TICKS;
    player_lives--;
    if (player_lives <= 0)
        state = STATE_TOD;
}

/** @brief Advances all alien bullets, checking shields and the player. */
static void alien_bullets_update(void)
{
    for (int i = 0; i < ALIEN_BULLETS_MAX; i++) {
        Bullet *b = &alien_bullets[i];
        if (!b->active)
            continue;

        b->y += ALIEN_BULLET_SPEED;
        if (b->y > FB8_HEIGHT) {
            b->active = 0;
            continue;
        }

        if (shield_hit_test(b->x, b->y, 2, 8)) {
            b->active = 0;
            continue;
        }

        if (player_explode_ticks == 0 &&
            rects_overlap(b->x, b->y, 2, 8, player_x, PLAYER_Y, 16, 16)) {
            b->active = 0;
            player_hit();
        }
    }
}

/**
 * @brief Analog steering speed for the current stick deflection: 0 inside
 *        the deadzone, scaling linearly up to +/-PLAYER_MAX_SPEED at full
 *        travel either side of center.
 */
static int analog_move_delta(void)
{
    int v = (int)joystick_get_adc_x() - JS_X_CENTER;

    if (v > -JS_X_DEADZONE && v < JS_X_DEADZONE)
        return 0;
    v += (v > 0) ? -JS_X_DEADZONE : JS_X_DEADZONE;

    int range = JS_X_CENTER - JS_X_DEADZONE;
    if (v > range) v = range;
    if (v < -range) v = -range;

    return (v * PLAYER_MAX_SPEED) / range;
}

/** @brief Runs one gameplay tick: input, aliens, bullets, UFO, collisions. */
static void play_tick(const JoystickState *js, uint32_t tick_ms)
{
    if (player_explode_ticks > 0) {
        player_explode_ticks--;
    } else {
        player_x += analog_move_delta();
        if (player_x < 0) player_x = 0;
        if (player_x > FB8_WIDTH - 16) player_x = FB8_WIDTH - 16;

        if ((js->pressed & JS_BTN) && !player_bullet.active) {
            player_bullet.active = 1;
            player_bullet.x = player_x + 7;
            player_bullet.y = PLAYER_Y - 8;
            sfx_play(SFX_LASER);
        }
    }

    if (alien_move_ticks_left > 0)
        alien_move_ticks_left--;
    if (alien_move_ticks_left == 0 && state == STATE_SPIEL) {
        int alive = alien_alive_count();
        int interval = 4 + (alive * 12) / (ALIEN_ROWS * ALIEN_COLS);
        step_formation();
        alien_move_ticks_left = (uint32_t)interval;
    }

    if (state != STATE_SPIEL)
        return;

    for (int r = 0; r < ALIEN_ROWS; r++)
        for (int c = 0; c < ALIEN_COLS; c++)
            if (aliens[r][c].explode_ticks > 0)
                aliens[r][c].explode_ticks--;

    alien_try_shoot();
    ufo_update(tick_ms);
    player_bullet_update();
    alien_bullets_update();

    if (alien_alive_count() == 0) {
        wave++;
        start_wave();
        say("NAECHSTE WELLE!");
    }
}

/** @brief GameAPI update callback. */
static void invaders_update(const JoystickState *js, uint32_t tick_ms)
{
    update_stars();

    if (message_ms_left > 0) {
        message_ms_left -= (int32_t)tick_ms;
        if (message_ms_left < 0)
            message_ms_left = 0;
    }

    switch (state) {
    case STATE_TITEL:
    case STATE_TOD:
        if (js->pressed & JS_BTN) {
            neues_spiel();
            state = STATE_SPIEL;
        }
        break;

    case STATE_SPIEL:
        play_tick(js, tick_ms);
        break;
    }
}

/** @brief Draws a line of centered-ish text (rough centering by char count). */
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
    draw_center(40,  "WELTRAUM-INVASOREN", 5);
    draw_center(76,  "VERTEIDIGE DIE ERDE VOR DEN ALIENS", 15);
    draw_center(110, "ANALOGSTICK: BEWEGEN", 1);
    draw_center(126, "KNOPF: SCHIESSEN", 1);
    draw_center(160, "NUTZE DIE SCHUTZSCHILDE UND", 2);
    draw_center(176, "ZERSTOERE JEDE WELLE ALIENS", 2);
    draw_center(210, "DRUECKE DEN KNOPF", 13);
}

static void draw_ende(void)
{
    char line[32];
    int p = 0;

    draw_center(90, "GAME OVER", 2);

    append(line, &p, "PUNKTE: ");
    char num[12];
    utoa10(score, num);
    append(line, &p, num);
    line[p] = 0;
    draw_center(112, line, 1);

    draw_center(150, "DRUECKE DEN KNOPF", 15);
}

static void draw_shields(void)
{
    for (int s = 0; s < SHIELD_COUNT; s++) {
        for (int r = 0; r < SHIELD_ROWS; r++) {
            for (int c = 0; c < SHIELD_COLS; c++) {
                if (!shield_cell[s][r][c])
                    continue;
                fb8_fill_rect(shield_x[s] + c * SHIELD_BLOCK,
                              SHIELD_Y + r * SHIELD_BLOCK,
                              SHIELD_BLOCK, SHIELD_BLOCK, 13);
            }
        }
    }
}

static void draw_aliens(void)
{
    for (int r = 0; r < ALIEN_ROWS; r++) {
        const sprite16_t *frame_a, *frame_b;
        if (r == 0)                    { frame_a = &alien_squid_a; frame_b = &alien_squid_b; }
        else if (r == ALIEN_ROWS - 1)  { frame_a = &alien_octo_a;  frame_b = &alien_octo_b;  }
        else                            { frame_a = &alien_crab_a;  frame_b = &alien_crab_b;  }

        for (int c = 0; c < ALIEN_COLS; c++) {
            int x, y;
            alien_pixel_pos(r, c, &x, &y);
            if (aliens[r][c].alive) {
                draw_sprite16(x, y, alien_anim_frame ? frame_b : frame_a);
            } else if (aliens[r][c].explode_ticks > 0) {
                draw_sprite16(x, y, &explosion_sprite);
            }
        }
    }
}

static void draw_hud(void)
{
    char line[48];
    int p = 0;
    char num[12];

    append(line, &p, "PUNKTE ");
    utoa10(score, num);
    append(line, &p, num);

    append(line, &p, "  WELLE ");
    utoa10((uint32_t)wave, num);
    append(line, &p, num);

    append(line, &p, "  LEBEN ");
    utoa10((uint32_t)(player_lives < 0 ? 0 : player_lives), num);
    append(line, &p, num);

    line[p] = 0;
    draw_text(4, 4, line, 1);

    if (message_ms_left > 0)
        draw_center(60, message, 7);
}

/** @brief GameAPI draw callback. */
static void invaders_draw(void)
{
    draw_stars();

    if (state == STATE_TITEL) { draw_titel(); return; }
    if (state == STATE_TOD)   { draw_ende(); return; }

    draw_shields();
    draw_aliens();

    if (ufo_active)
        draw_sprite16(ufo_x, UFO_Y, &ufo_sprite);

    if (player_bullet.active)
        fb8_fill_rect(player_bullet.x, player_bullet.y, 2, 8, 1);

    for (int i = 0; i < ALIEN_BULLETS_MAX; i++)
        if (alien_bullets[i].active)
            fb8_fill_rect(alien_bullets[i].x, alien_bullets[i].y, 2, 8, 10);

    if (player_explode_ticks > 0)
        draw_sprite16(player_x, PLAYER_Y, &explosion_sprite);
    else
        draw_sprite16(player_x, PLAYER_Y, &player_sprite);

    draw_hud();
}

static const GameAPI invaders_game = {
    .name   = "invaders",
    .init   = invaders_init,
    .update = invaders_update,
    .draw   = invaders_draw,
};

/** @brief Entry point: runs Invaders via the generic GameAPI loop. */
int main(void)
{
    game_run(&invaders_game, TICK_MS);
    return 0;
}
