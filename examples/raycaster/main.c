/**
 * @file main.c
 * @brief "Raycaster" - a Wolfenstein-3D-style tech demo built on the
 *        generic GameAPI. Casts one ray per screen column with the
 *        classic DDA algorithm against a small fixed wall grid (map.h)
 *        and draws each column as a vertical strip, plus flat floor/
 *        ceiling colors and a top-down minimap in the corner. A handful
 *        of billboard sprites (items, patrolling enemies) live in the
 *        same space, scaled by distance and depth-tested against the
 *        walls' per-column z-buffer.
 *
 * No win/lose condition, combat, or enemy AI beyond back-and-forth
 * patrolling - this exists to answer "is this platform enough for a
 * simple 3D engine", not to be a game.
 *
 * Controls: UP/DOWN walk forward/backward, LEFT/RIGHT turn in place.
 * Walking into an item collects it; walking into an enemy triggers a
 * brief red screen flash.
 */
#include "game.h"
#include "framebuffer8.h"
#include "sprite16.h"
#include "joystick.h"
#include "sfx.h"
#include "text.h"
#include "map.h"

#define TICK_MS  30

#define MOVE_SPEED  0.09f  /* map cells per tick */
#define ROT_COS     0.99756f  /* cos(4 deg): per-tick turn step */
#define ROT_SIN     0.06976f  /* sin(4 deg) */

#define CEILING_COLOR  14  /* light blue */
#define FLOOR_COLOR     9  /* brown */

#define PLAYER_START_X  2.5f
#define PLAYER_START_Y  1.5f

#define MINIMAP_SCALE  3  /* px per map cell */
#define MINIMAP_MARGIN 4

#define ITEM_COUNT   3
#define ENEMY_COUNT  2

#define PICKUP_RADIUS      0.5f
#define ENEMY_HIT_RADIUS   0.5f
#define ENEMY_SPEED        0.02f  /* patrol param t per tick */
#define HIT_FLASH_TICKS    6
#define HIT_FLASH_COLOR    2      /* red */

/* Player position (map cell units) and facing, Lode-Vandenberghe style:
 * dir is the view direction, plane is the camera plane (perpendicular to
 * dir, its length sets the FOV - ~0.66 gives roughly 66 degrees here). */
static float pos_x, pos_y;
static float dir_x, dir_y;
static float plane_x, plane_y;

static uint8_t footstep_ticks;
static uint8_t hit_flash_ticks;
static uint32_t items_collected;

/** @brief One perpendicular wall distance per screen column, filled by
 *         cast_column() and read by render_billboard() for occlusion. */
static float z_buffer[FB8_WIDTH];

typedef struct {
    float x, y;
    uint8_t active;
} Item;

typedef struct {
    float ax, ay;  /* patrol endpoint A */
    float bx, by;  /* patrol endpoint B */
    float t;       /* 0..1 position along A->B */
    int   dir;     /* +1 towards B, -1 towards A */
    float x, y;    /* current position, derived from t */
} Enemy;

/** @brief One sprite ready to be depth-sorted and drawn (see draw_billboards()). */
typedef struct {
    float x, y, dist2;
    const sprite16_t *tex;
} BillboardEntry;

/* Positions hand-picked against world_map (map.c): every coordinate below
 * (and, for enemies, every cell on the straight line between ax/ay and
 * bx/by) sits on an open (0) floor cell. */
static Item items[ITEM_COUNT] = {
    { 12.5f, 3.5f,  1 },
    {  7.5f, 10.5f, 1 },
    { 13.5f, 13.5f, 1 },
};

static Enemy enemies[ENEMY_COUNT] = {
    { 4.5f, 4.5f,  8.5f, 4.5f,  0.0f, 1, 4.5f, 4.5f },
    { 12.5f, 6.5f, 12.5f, 9.5f, 0.0f, 1, 12.5f, 6.5f },
};

/* clang-format off */
static const sprite16_t item_sprite = {
    .px = {
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,7,7,7,7,7,7,0,0,0,0,0},
        {0,0,0,7,7,7,7,7,7,7,7,7,7,0,0,0},
        {0,0,7,7,7,7,7,7,7,7,7,7,7,7,0,0},
        {0,7,7,7,7,1,7,7,7,7,7,7,7,7,7,0},
        {0,7,7,7,7,7,7,7,7,7,7,7,7,7,7,0},
        {0,7,7,7,7,7,7,7,7,7,7,7,7,7,7,0},
        {0,7,7,7,7,7,7,7,7,7,7,7,7,7,7,0},
        {0,7,7,7,7,7,7,7,7,7,7,7,7,7,7,0},
        {0,0,7,7,7,7,7,7,7,7,7,7,7,7,0,0},
        {0,0,0,7,7,7,7,7,7,7,7,7,7,0,0,0},
        {0,0,0,0,0,7,7,7,7,7,7,0,0,0,0,0},
        {0,0,0,0,0,0,0,7,7,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    }
};

static const sprite16_t enemy_sprite = {
    .px = {
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,2,2,2,2,2,2,0,0,0,0,0},
        {0,0,0,2,2,2,2,2,2,2,2,2,2,0,0,0},
        {0,0,2,2,2,2,2,2,2,2,2,2,2,2,0,0},
        {0,2,2,2,2,2,1,2,2,1,2,2,2,2,2,0},
        {0,2,2,2,2,2,2,2,2,2,2,2,2,2,2,0},
        {0,2,2,2,2,2,2,2,2,2,2,2,2,2,2,0},
        {0,2,2,2,2,2,2,2,2,2,2,2,2,2,2,0},
        {0,2,2,2,2,2,9,9,9,9,2,2,2,2,2,0},
        {0,0,2,2,2,2,2,2,2,2,2,2,2,2,0,0},
        {0,0,0,2,2,2,2,2,2,2,2,2,2,0,0,0},
        {0,0,0,0,0,2,2,2,2,2,2,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    }
};
/* clang-format on */

/** @brief Rotates vector (x,y) in place by the angle whose cos/sin are given. */
static void rotate_vec(float *x, float *y, float cos_a, float sin_a)
{
    float ox = *x;
    *x = ox * cos_a - *y * sin_a;
    *y = ox * sin_a + *y * cos_a;
}

static float fabs_f(float v)
{
    return v < 0.0f ? -v : v;
}

static int abs_i(int v)
{
    return v < 0 ? -v : v;
}

/** @brief Minimal unsigned-int-to-decimal writer (no sprintf under -nostdlib).
 *  @return Number of characters written (not NUL-terminated). */
static int write_uint(char *out, uint32_t v)
{
    char tmp[12];
    int i = 0;
    if (v == 0) {
        out[0] = '0';
        return 1;
    }
    while (v > 0 && i < 11) {
        tmp[i++] = (char)('0' + (v % 10));
        v /= 10;
    }
    for (int j = 0; j < i; j++)
        out[j] = tmp[i - 1 - j];
    return i;
}

/** @brief GameAPI init callback. */
static void raycaster_init(void)
{
    sfx_init();

    pos_x = PLAYER_START_X;
    pos_y = PLAYER_START_Y;
    dir_x = 1.0f;
    dir_y = 0.0f;
    plane_x = 0.0f;
    plane_y = 0.66f;
}

/** @brief GameAPI update callback: turning, walking, wall collision. */
static void raycaster_update(const JoystickState *js, uint32_t tick_ms)
{
    (void)tick_ms;

    if (js->raw & JS_RIGHT) {
        rotate_vec(&dir_x, &dir_y, ROT_COS, ROT_SIN);
        rotate_vec(&plane_x, &plane_y, ROT_COS, ROT_SIN);
    }
    if (js->raw & JS_LEFT) {
        rotate_vec(&dir_x, &dir_y, ROT_COS, -ROT_SIN);
        rotate_vec(&plane_x, &plane_y, ROT_COS, -ROT_SIN);
    }

    int move_dir = 0;
    if (js->raw & JS_UP)   move_dir += 1;
    if (js->raw & JS_DOWN) move_dir -= 1;

    if (move_dir != 0) {
        /* Axis-separated collision: only apply the move on an axis if the
         * destination cell on that axis is open. Lets the player slide
         * along a wall instead of stopping dead on any diagonal contact. */
        float new_x = pos_x + dir_x * MOVE_SPEED * move_dir;
        float new_y = pos_y + dir_y * MOVE_SPEED * move_dir;

        if (world_map[(int)pos_y][(int)new_x] == 0)
            pos_x = new_x;
        if (world_map[(int)new_y][(int)pos_x] == 0)
            pos_y = new_y;

        if (++footstep_ticks >= 6) {
            footstep_ticks = 0;
            sfx_play(SFX_MOVE);
        }
    } else {
        footstep_ticks = 0;
    }

    for (int i = 0; i < ENEMY_COUNT; i++) {
        Enemy *e = &enemies[i];
        e->t += ENEMY_SPEED * e->dir;
        if (e->t >= 1.0f) { e->t = 1.0f; e->dir = -1; }
        if (e->t <= 0.0f) { e->t = 0.0f; e->dir = 1; }
        e->x = e->ax + (e->bx - e->ax) * e->t;
        e->y = e->ay + (e->by - e->ay) * e->t;
    }

    for (int i = 0; i < ITEM_COUNT; i++) {
        Item *it = &items[i];
        if (!it->active)
            continue;
        float dx = pos_x - it->x;
        float dy = pos_y - it->y;
        if (dx * dx + dy * dy < PICKUP_RADIUS * PICKUP_RADIUS) {
            it->active = 0;
            items_collected++;
            sfx_play(SFX_PICKUP);
        }
    }

    for (int i = 0; i < ENEMY_COUNT; i++) {
        Enemy *e = &enemies[i];
        float dx = pos_x - e->x;
        float dy = pos_y - e->y;
        if (dx * dx + dy * dy < ENEMY_HIT_RADIUS * ENEMY_HIT_RADIUS
            && hit_flash_ticks == 0) {
            hit_flash_ticks = HIT_FLASH_TICKS;
            sfx_play(SFX_EXPLOSION);
        }
    }

    if (hit_flash_ticks > 0)
        hit_flash_ticks--;
}

/* clang-format off */
/** @brief Wall type 1: brick, running-bond coursing (offset every other row). */
static const uint8_t brick_tex[16][16] = {
    { 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12,  9,  9,  9,  9,  9,  9,  9, 12, 10, 10, 10, 10, 10, 10, 10 },
    { 12,  9,  9,  9,  9,  9,  9,  9, 12, 10, 10, 10, 10, 10, 10, 10 },
    { 12,  9,  9,  9,  9,  9,  9,  9, 12, 10, 10, 10, 10, 10, 10, 10 },
    { 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12 },
    {  9,  9,  9,  9, 12, 10, 10, 10, 10, 10, 10, 10, 12,  9,  9,  9 },
    {  9,  9,  9,  9, 12, 10, 10, 10, 10, 10, 10, 10, 12,  9,  9,  9 },
    {  9,  9,  9,  9, 12, 10, 10, 10, 10, 10, 10, 10, 12,  9,  9,  9 },
    { 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12,  9,  9,  9,  9,  9,  9,  9, 12, 10, 10, 10, 10, 10, 10, 10 },
    { 12,  9,  9,  9,  9,  9,  9,  9, 12, 10, 10, 10, 10, 10, 10, 10 },
    { 12,  9,  9,  9,  9,  9,  9,  9, 12, 10, 10, 10, 10, 10, 10, 10 },
    { 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12 },
    {  9,  9,  9,  9, 12, 10, 10, 10, 10, 10, 10, 10, 12,  9,  9,  9 },
    {  9,  9,  9,  9, 12, 10, 10, 10, 10, 10, 10, 10, 12,  9,  9,  9 },
    {  9,  9,  9,  9, 12, 10, 10, 10, 10, 10, 10, 10, 12,  9,  9,  9 },
};

/** @brief Wall type 2: large stone blocks in a 2x2 grid per tile. */
static const uint8_t stone_tex[16][16] = {
    { 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 15, 15, 15, 15, 15, 15, 15, 11, 12, 12, 12, 12, 12, 12, 12 },
    { 11, 15, 15, 15, 15, 15, 15, 15, 11, 12, 12, 12, 12, 12, 12, 12 },
    { 11, 15, 15, 15, 15, 15, 15, 15, 11, 12, 12, 12, 12, 12, 12, 12 },
    { 11, 15, 15, 15, 15, 15, 15, 15, 11, 12, 12, 12, 12, 12, 12, 12 },
    { 11, 15, 15, 15, 15, 15, 15, 15, 11, 12, 12, 12, 12, 12, 12, 12 },
    { 11, 15, 15, 15, 15, 15, 15, 15, 11, 12, 12, 12, 12, 12, 12, 12 },
    { 11, 15, 15, 15, 15, 15, 15, 15, 11, 12, 12, 12, 12, 12, 12, 12 },
    { 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 12, 12, 12, 12, 12, 12, 12, 11, 15, 15, 15, 15, 15, 15, 15 },
    { 11, 12, 12, 12, 12, 12, 12, 12, 11, 15, 15, 15, 15, 15, 15, 15 },
    { 11, 12, 12, 12, 12, 12, 12, 12, 11, 15, 15, 15, 15, 15, 15, 15 },
    { 11, 12, 12, 12, 12, 12, 12, 12, 11, 15, 15, 15, 15, 15, 15, 15 },
    { 11, 12, 12, 12, 12, 12, 12, 12, 11, 15, 15, 15, 15, 15, 15, 15 },
    { 11, 12, 12, 12, 12, 12, 12, 12, 11, 15, 15, 15, 15, 15, 15, 15 },
    { 11, 12, 12, 12, 12, 12, 12, 12, 11, 15, 15, 15, 15, 15, 15, 15 },
};

/** @brief Wall type 3: vertical wood planks with alternating grain shade. */
static const uint8_t wood_tex[16][16] = {
    { 0, 8, 8, 8, 0, 9, 9, 9, 0, 8, 8, 8, 0, 9, 9, 9 },
    { 0, 9, 9, 9, 0, 8, 8, 8, 0, 9, 9, 9, 0, 8, 8, 8 },
    { 0, 9, 9, 9, 0, 8, 8, 8, 0, 9, 9, 9, 0, 8, 8, 8 },
    { 0, 9, 9, 9, 0, 8, 8, 8, 0, 9, 9, 9, 0, 8, 8, 8 },
    { 0, 9, 9, 9, 0, 8, 8, 8, 0, 9, 9, 9, 0, 8, 8, 8 },
    { 0, 8, 8, 8, 0, 9, 9, 9, 0, 8, 8, 8, 0, 9, 9, 9 },
    { 0, 9, 9, 9, 0, 8, 8, 8, 0, 9, 9, 9, 0, 8, 8, 8 },
    { 0, 9, 9, 9, 0, 8, 8, 8, 0, 9, 9, 9, 0, 8, 8, 8 },
    { 0, 9, 9, 9, 0, 8, 8, 8, 0, 9, 9, 9, 0, 8, 8, 8 },
    { 0, 9, 9, 9, 0, 8, 8, 8, 0, 9, 9, 9, 0, 8, 8, 8 },
    { 0, 8, 8, 8, 0, 9, 9, 9, 0, 8, 8, 8, 0, 9, 9, 9 },
    { 0, 9, 9, 9, 0, 8, 8, 8, 0, 9, 9, 9, 0, 8, 8, 8 },
    { 0, 9, 9, 9, 0, 8, 8, 8, 0, 9, 9, 9, 0, 8, 8, 8 },
    { 0, 9, 9, 9, 0, 8, 8, 8, 0, 9, 9, 9, 0, 8, 8, 8 },
    { 0, 9, 9, 9, 0, 8, 8, 8, 0, 9, 9, 9, 0, 8, 8, 8 },
    { 0, 8, 8, 8, 0, 9, 9, 9, 0, 8, 8, 8, 0, 9, 9, 9 },
};
/* clang-format on */

/**
 * @brief One-step-darker palette index for each of the 16 base colors -
 *        applied to every texel on an E/W-facing wall so the same bitmap
 *        still gives the classic two-tone side shading instead of reading
 *        flat regardless of orientation. Colors with no darker relative in
 *        the 16-entry base palette (e.g. pure hues with only one shade)
 *        fall back to black.
 */
static const uint8_t shade_dark[16] = {
     0, 15,  9,  6,  0,  0,  0,  8,
     9,  0,  2,  0, 11,  5,  6, 12,
};

static uint8_t sample_wall_tex(int cell_type, int tex_x, int tex_y)
{
    switch (cell_type) {
    case 1:  return brick_tex[tex_y][tex_x];
    case 2:  return stone_tex[tex_y][tex_x];
    case 3:  return wood_tex[tex_y][tex_x];
    default: return 0;
    }
}

/** @brief Casts one column's ray and draws its wall strip. */
static void cast_column(int x)
{
    float camera_x = 2.0f * x / (float)FB8_WIDTH - 1.0f;
    float ray_dir_x = dir_x + plane_x * camera_x;
    float ray_dir_y = dir_y + plane_y * camera_x;

    int map_x = (int)pos_x;
    int map_y = (int)pos_y;

    /* ray_dir_x/y == 0 gives IEEE-754 +inf here, which the DDA loop below
     * handles correctly on its own (that axis just never gets stepped) -
     * no special-case needed. */
    float delta_dist_x = fabs_f(1.0f / ray_dir_x);
    float delta_dist_y = fabs_f(1.0f / ray_dir_y);

    int step_x, step_y;
    float side_dist_x, side_dist_y;

    if (ray_dir_x < 0.0f) {
        step_x = -1;
        side_dist_x = (pos_x - map_x) * delta_dist_x;
    } else {
        step_x = 1;
        side_dist_x = (map_x + 1.0f - pos_x) * delta_dist_x;
    }
    if (ray_dir_y < 0.0f) {
        step_y = -1;
        side_dist_y = (pos_y - map_y) * delta_dist_y;
    } else {
        step_y = 1;
        side_dist_y = (map_y + 1.0f - pos_y) * delta_dist_y;
    }

    int side = 0, hit = 0, cell_type = 0;

    /* Step cap keeps map_x/map_y in-bounds even in a scenario the bordered
     * map shouldn't allow (e.g. player somehow outside it) - every step
     * checks bounds before the array read below. */
    for (int steps = 0; steps < 48 && !hit; steps++) {
        if (side_dist_x < side_dist_y) {
            side_dist_x += delta_dist_x;
            map_x += step_x;
            side = 0;
        } else {
            side_dist_y += delta_dist_y;
            map_y += step_y;
            side = 1;
        }

        if (map_x < 0 || map_x >= MAP_W || map_y < 0 || map_y >= MAP_H)
            break;

        if (world_map[map_y][map_x] != 0) {
            cell_type = world_map[map_y][map_x];
            hit = 1;
        }
    }

    if (!hit) {
        z_buffer[x] = 1e30f;  /* nothing hit: never occlude a sprite here */
        return;
    }

    float perp_dist = (side == 0) ? (side_dist_x - delta_dist_x)
                                   : (side_dist_y - delta_dist_y);
    if (perp_dist < 0.01f)
        perp_dist = 0.01f;

    z_buffer[x] = perp_dist;

    int line_height = (int)(FB8_HEIGHT / perp_dist);
    if (line_height < 1)
        line_height = 1;  /* guards the tex_step division below at extreme distance */

    int draw_start_raw = -line_height / 2 + FB8_HEIGHT / 2;
    int draw_end_raw   =  line_height / 2 + FB8_HEIGHT / 2;

    /* Clamp before the pixel loop below: an unclamped huge line_height
     * (player standing right next to a wall) would otherwise burn
     * thousands of wasted iterations on a single column. */
    int draw_start = draw_start_raw < 0 ? 0 : draw_start_raw;
    int draw_end   = draw_end_raw >= FB8_HEIGHT ? FB8_HEIGHT - 1 : draw_end_raw;

    /* Texture u: exactly where (0..1 across the cell face) the ray hit,
     * taken from whichever world coordinate runs *along* the wall. */
    float wall_x = (side == 0) ? (pos_y + perp_dist * ray_dir_y)
                                : (pos_x + perp_dist * ray_dir_x);
    wall_x -= (float)(int)wall_x;  /* fractional part; map coords are >= 0 */

    int tex_x = (int)(wall_x * 16.0f);
    if (tex_x < 0)  tex_x = 0;
    if (tex_x > 15) tex_x = 15;
    /* Without this, the texture reads mirrored on two of the four wall
     * orientations (a ray can hit the same world edge from either side). */
    if ((side == 0 && ray_dir_x > 0.0f) || (side == 1 && ray_dir_y < 0.0f))
        tex_x = 15 - tex_x;

    /* Texture v steps through the *unclamped* line height so a wall that's
     * partly off-screen still samples the right rows once clamped. */
    float tex_step = 16.0f / (float)line_height;
    float tex_pos = (float)(draw_start - draw_start_raw) * tex_step;

    for (int y = draw_start; y <= draw_end; y++) {
        int tex_y = ((int)tex_pos) & 15;
        tex_pos += tex_step;

        uint8_t texel = sample_wall_tex(cell_type, tex_x, tex_y);
        uint8_t color = (side == 1) ? shade_dark[texel] : texel;
        /* Direct write, not fb8_set_pixel(): x/y are already known in-bounds
         * from the clamps above, same trade-off fb8_fill_rect() makes. */
        framebuffer8[y * FB8_WIDTH + x] = color;
    }
}

/**
 * @brief Draws a 16x16 sprite as a distance-scaled, depth-tested billboard,
 *        classic raycaster sprite casting: transform the sprite's world
 *        position into camera space via the inverse of the dir/plane
 *        matrix, size it on screen by that depth, then sample the source
 *        texture per column/row (nearest-neighbor) and skip any pixel
 *        whose column is behind a nearer wall or sprite (z_buffer test).
 */
static void render_billboard(float x, float y, const sprite16_t *tex)
{
    float rel_x = x - pos_x;
    float rel_y = y - pos_y;

    float inv_det = 1.0f / (plane_x * dir_y - dir_x * plane_y);
    float transform_x = inv_det * (dir_y * rel_x - dir_x * rel_y);
    float transform_y = inv_det * (-plane_y * rel_x + plane_x * rel_y);

    if (transform_y <= 0.05f)
        return;  /* behind (or right on top of) the camera */

    int screen_x = (int)((FB8_WIDTH / 2) * (1.0f + transform_x / transform_y));
    int size = abs_i((int)(FB8_HEIGHT / transform_y));
    if (size <= 0)
        return;

    int draw_start_x = screen_x - size / 2;
    int draw_start_y = FB8_HEIGHT / 2 - size / 2;

    int clip_x0 = draw_start_x < 0 ? 0 : draw_start_x;
    int clip_x1 = draw_start_x + size >= FB8_WIDTH ? FB8_WIDTH - 1 : draw_start_x + size - 1;
    int clip_y0 = draw_start_y < 0 ? 0 : draw_start_y;
    int clip_y1 = draw_start_y + size >= FB8_HEIGHT ? FB8_HEIGHT - 1 : draw_start_y + size - 1;

    for (int sx = clip_x0; sx <= clip_x1; sx++) {
        if (transform_y >= z_buffer[sx])
            continue;  /* a wall (or a nearer sprite column) occludes this */

        int tex_x = (sx - draw_start_x) * 16 / size;
        if (tex_x < 0) tex_x = 0;
        if (tex_x > 15) tex_x = 15;

        for (int sy = clip_y0; sy <= clip_y1; sy++) {
            int tex_y = (sy - draw_start_y) * 16 / size;
            if (tex_y < 0) tex_y = 0;
            if (tex_y > 15) tex_y = 15;

            uint8_t c = tex->px[tex_y][tex_x];
            if (c == 0)
                continue;  /* transparent */
            fb8_set_pixel(sx, sy, c);
        }
    }
}

/** @brief Draws all active items/enemies, farthest first so nearer ones
 *         correctly paint over farther ones where they overlap. */
static void draw_billboards(void)
{
    /* At most ITEM_COUNT + ENEMY_COUNT entries - plain insertion sort by
     * squared distance to the player is more than fast enough here. */
    BillboardEntry list[ITEM_COUNT + ENEMY_COUNT];
    int n = 0;

    for (int i = 0; i < ITEM_COUNT; i++) {
        if (!items[i].active)
            continue;
        float dx = items[i].x - pos_x, dy = items[i].y - pos_y;
        list[n].x = items[i].x;
        list[n].y = items[i].y;
        list[n].dist2 = dx * dx + dy * dy;
        list[n].tex = &item_sprite;
        n++;
    }
    for (int i = 0; i < ENEMY_COUNT; i++) {
        float dx = enemies[i].x - pos_x, dy = enemies[i].y - pos_y;
        list[n].x = enemies[i].x;
        list[n].y = enemies[i].y;
        list[n].dist2 = dx * dx + dy * dy;
        list[n].tex = &enemy_sprite;
        n++;
    }

    for (int i = 1; i < n; i++) {
        BillboardEntry key = list[i];
        int j = i - 1;
        while (j >= 0 && list[j].dist2 < key.dist2) {
            list[j + 1] = list[j];
            j--;
        }
        list[j + 1] = key;
    }

    for (int i = 0; i < n; i++)
        render_billboard(list[i].x, list[i].y, list[i].tex);
}

/** @brief Draws the top-down minimap overlay in the top-right corner. */
static void draw_minimap(void)
{
    int mm_x = FB8_WIDTH - MAP_W * MINIMAP_SCALE - MINIMAP_MARGIN;
    int mm_y = MINIMAP_MARGIN;

    fb8_fill_rect(mm_x - 1, mm_y - 1,
                  MAP_W * MINIMAP_SCALE + 2, MAP_H * MINIMAP_SCALE + 2, 0);

    for (int y = 0; y < MAP_H; y++) {
        for (int x = 0; x < MAP_W; x++) {
            uint8_t cell = world_map[y][x];
            if (cell == 0)
                continue;
            fb8_fill_rect(mm_x + x * MINIMAP_SCALE, mm_y + y * MINIMAP_SCALE,
                          MINIMAP_SCALE, MINIMAP_SCALE,
                          wall_colors[cell - 1][0]);
        }
    }

    for (int i = 0; i < ITEM_COUNT; i++) {
        if (!items[i].active)
            continue;
        fb8_fill_rect(mm_x + (int)(items[i].x * MINIMAP_SCALE) - 1,
                      mm_y + (int)(items[i].y * MINIMAP_SCALE) - 1, 2, 2, 7);
    }
    for (int i = 0; i < ENEMY_COUNT; i++) {
        fb8_fill_rect(mm_x + (int)(enemies[i].x * MINIMAP_SCALE) - 1,
                      mm_y + (int)(enemies[i].y * MINIMAP_SCALE) - 1, 2, 2, 2);
    }

    int px = mm_x + (int)(pos_x * MINIMAP_SCALE);
    int py = mm_y + (int)(pos_y * MINIMAP_SCALE);
    fb8_fill_rect(px - 1, py - 1, 2, 2, 1 /* white */);

    for (int i = 1; i <= 4; i++)
        fb8_set_pixel(px + (int)(dir_x * i), py + (int)(dir_y * i), 1);
}

/** @brief Draws the "ITEMS n/N" counter in the top-left corner. */
static void draw_hud(void)
{
    char line[16] = "ITEMS ";
    int p = 6;
    p += write_uint(line + p, items_collected);
    line[p++] = '/';
    p += write_uint(line + p, ITEM_COUNT);
    line[p] = 0;
    draw_text(4, 4, line, 1);
}

/** @brief GameAPI draw callback. */
static void raycaster_draw(void)
{
    if (hit_flash_ticks > 0) {
        fb8_fill_rect(0, 0, FB8_WIDTH, FB8_HEIGHT, HIT_FLASH_COLOR);
        return;
    }

    fb8_fill_rect(0, 0, FB8_WIDTH, FB8_HEIGHT / 2, CEILING_COLOR);
    fb8_fill_rect(0, FB8_HEIGHT / 2, FB8_WIDTH, FB8_HEIGHT - FB8_HEIGHT / 2,
                  FLOOR_COLOR);

    for (int x = 0; x < FB8_WIDTH; x++)
        cast_column(x);

    draw_billboards();
    draw_minimap();
    draw_hud();
}

static const GameAPI raycaster_game = {
    .name   = "raycaster",
    .init   = raycaster_init,
    .update = raycaster_update,
    .draw   = raycaster_draw,
};

/** @brief Entry point: runs the raycaster demo via the generic GameAPI loop. */
int main(void)
{
    game_run(&raycaster_game, TICK_MS);
    return 0;
}
