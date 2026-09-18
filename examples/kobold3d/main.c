/**
 * @file main.c
 * @brief "Der Kobold", reimagined as a walkable first-person scene on the
 *        raycaster engine (see examples/raycaster) instead of
 *        examples/kobold's cell-to-cell text-adventure paging. Same story,
 *        same 3x3 randomized forest, same ring puzzle and win/lose
 *        endings (content/logic copied near-verbatim from
 *        examples/kobold/main.c - see story.h/story.c, unchanged), but
 *        walked through in 3D. Story photos load from the external
 *        W25Q128 flash exactly as in examples/kobold (see art.c) -
 *        already uploaded there by an earlier session, no re-upload here.
 *
 * Unlike examples/kobold, stations aren't triggered by pressing BTN: each
 * non-forest station's photo/text/choices appear automatically the moment
 * the player walks into that cell (a "cutscene" interstitial), which is
 * what makes this feel like walking through the story instead of paging
 * through it. Plain STATION_WALD cells deliberately trigger nothing (else
 * every ordinary forest step would pop an interstitial).
 *
 * Controls: UP/DOWN walk forward/backward, LEFT/RIGHT turn in place, BTN
 * confirms choices on an interstitial screen.
 */
#include "game.h"
#include "framebuffer8.h"
#include "joystick.h"
#include "sfx.h"
#include "music.h"
#include "text.h"
#include "art.h"
#include "story.h"
#include "track.h"
#include "map.h"
#include "w25q128.h"

#define TICK_MS  30

#define MOVE_SPEED  0.07f      /* map cells per tick */
#define ROT_COS     0.99756f   /* cos(4 deg): per-tick turn step */
#define ROT_SIN     0.06976f   /* sin(4 deg) */

#define CEILING_COLOR  14  /* light blue */
#define FLOOR_COLOR     9  /* brown */
#define MORTAR_COLOR   11  /* dark grey: grout lines in the hedge texture */

#define ENTRANCE_X 1
#define ENTRANCE_Y 0

/* Player starts at the far end of the entrance corridor (see map.c),
 * facing south into it. */
#define PLAYER_START_X  ((float)(FOREST_ORIGIN_X + ENTRANCE_X) + 0.5f)
#define PLAYER_START_Y  1.5f

#define TEXT_X          4
#define TEXT_Y0         86
#define TEXT_LINE_H     11
#define TEXT_MAX_CHARS  38

#define CHOICE_X        4
#define CHOICE_Y0       168
#define CHOICE_LINE_H   12

#define HUD_Y          228

typedef enum {
    STATE_TITEL,
    STATE_FOREST,        /* 3D walking */
    STATE_INTERSTITIAL,  /* a full-screen photo + text + 1+ choices */
    STATE_ENDE,
} GameState;

/** @brief What confirming a STATE_INTERSTITIAL screen's selected choice does. */
typedef enum {
    ACTION_DISMISS,
    ACTION_ENTER_FOREST,
    ACTION_RING_WEAR,
    ACTION_RING_POCKET,
    ACTION_GIVE_RING,
    ACTION_CHEER_UP,
    ACTION_ATTACK,
} TextAction;

#define MAX_TEXT_CHOICES 4

static GameState state;

static float pos_x, pos_y;
static float dir_x, dir_y;
static float plane_x, plane_y;

static StationType forest[FOREST_H][FOREST_W];
static int forest_entered;  /* crossed from the corridor into the room once */
static int sealed;          /* forest entry confirmed: corridor is closed */
static int last_room_cell;  /* -1, or fy*FOREST_W+fx of the last cell that
                              * triggered a station, for re-trigger suppression */
static float step_accum;    /* distance walked inside the room since the
                              * last whole "step" (see turns below) */
static int turns;

static int has_ring, ring_taken, cheered_up;

static int ende_is_win;
static const char *ende_text;
static bg_id_t ende_bg;

static const char *text_body;
static int text_num_choices;
static const char *text_choice_label[MAX_TEXT_CHOICES];
static TextAction text_choice_action[MAX_TEXT_CHOICES];
static int cursor;

static uint32_t rng_state = 0xA5A5A5A5u;

/* ------------------------------------------------------------------- */
/* Small freestanding string helpers (no libc under -nostdlib).        */
/* ------------------------------------------------------------------- */

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

static void append(char *dst, int *pos, const char *s)
{
    while (*s)
        dst[(*pos)++] = *s++;
}

static uint32_t rng_next(void)
{
    rng_state = rng_state * 1664525u + 1013904223u;
    return rng_state;
}

static float fabs_f(float v)
{
    return v < 0.0f ? -v : v;
}

/* ------------------------------------------------------------------- */
/* Text layout helpers (copied from examples/kobold/main.c unchanged). */
/* ------------------------------------------------------------------- */

/** @brief Draws text with a full 1px black outline for legibility over
 *         the full-screen photo backgrounds. */
static void draw_text_shadow(int x, int y, const char *s, uint8_t color)
{
    static const int8_t dx[8] = { -1, 0, 1, -1, 1, -1, 0, 1 };
    static const int8_t dy[8] = { -1, -1, -1, 0, 0, 1, 1, 1 };

    for (int i = 0; i < 8; i++)
        draw_text(x + dx[i], y + dy[i], s, 0);
    draw_text(x, y, s, color);
}

/** @brief Greedy word-wrap + draw, honoring explicit '\n' breaks too. */
static void draw_wrapped(int x, int y, int max_chars, int line_h, const char *s, uint8_t color)
{
    int i = 0;
    int n = 0;
    while (s[n]) n++;
    int line_y = y;

    while (i < n) {
        int line_start = i;
        int last_space = -1;
        int col = 0;
        while (i < n && s[i] != '\n' && col < max_chars) {
            if (s[i] == ' ') last_space = i;
            i++; col++;
        }

        int line_end, next_i;
        if (i < n && s[i] == '\n') {
            line_end = i; next_i = i + 1;
        } else if (i >= n) {
            line_end = i; next_i = i;
        } else if (last_space >= line_start) {
            line_end = last_space; next_i = last_space + 1;
        } else {
            line_end = i; next_i = i;
        }

        char buf[48];
        int len = line_end - line_start;
        if (len > 47) len = 47;
        for (int k = 0; k < len; k++) buf[k] = s[line_start + k];
        buf[len] = 0;
        draw_text_shadow(x, line_y, buf, color);

        line_y += line_h;
        i = next_i;
    }
}

static void draw_center(int y, const char *s, uint8_t color)
{
    int len = 0;
    while (s[len]) len++;
    int x = (FB8_WIDTH - len * 8) / 2;
    if (x < 0) x = 0;
    draw_text_shadow(x, y, s, color);
}

/** @brief Draws one menu line, highlighting it with a ">" if selected. */
static void draw_choice_line(int y, const char *text, int selected)
{
    char buf[40];
    int p = 0;
    append(buf, &p, selected ? "> " : "  ");
    append(buf, &p, text);
    buf[p] = 0;
    draw_text_shadow(CHOICE_X, y, buf, selected ? 7 : 1);
}

/** @brief UP/DOWN cursor movement for a STATE_INTERSTITIAL choice menu. */
static void move_cursor(const JoystickState *js, int count)
{
    uint8_t nav = (uint8_t)(js->pressed | js->repeat);
    if ((nav & JS_UP) && cursor > 0) {
        cursor--;
        sfx_play(SFX_MOVE);
    }
    if ((nav & JS_DOWN) && cursor < count - 1) {
        cursor++;
        sfx_play(SFX_MOVE);
    }
}

/* ------------------------------------------------------------------- */
/* Story/game logic - near-verbatim from examples/kobold/main.c, just    */
/* triggered by walking into a cell instead of a BTN press.              */
/* ------------------------------------------------------------------- */

/** @brief Fisher-Yates shuffle of a small int array. */
static void shuffle(int *arr, int n)
{
    for (int i = n - 1; i > 0; i--) {
        int j = (int)(rng_next() % (uint32_t)(i + 1));
        int tmp = arr[i];
        arr[i] = arr[j];
        arr[j] = tmp;
    }
}

/** @brief Places the four stations into four of the eight non-entrance
 *         cells at random - a fresh layout every new game. */
static void shuffle_stations(void)
{
    for (int y = 0; y < FOREST_H; y++)
        for (int x = 0; x < FOREST_W; x++)
            forest[y][x] = STATION_WALD;

    int cells[FOREST_W * FOREST_H - 1];
    int n = 0;
    for (int y = 0; y < FOREST_H; y++)
        for (int x = 0; x < FOREST_W; x++)
            if (!(x == ENTRANCE_X && y == ENTRANCE_Y))
                cells[n++] = y * FOREST_W + x;

    shuffle(cells, n);

    static const StationType placed[4] = {
        STATION_HAUS, STATION_KOBOLD, STATION_MOOR, STATION_BACH
    };
    for (int i = 0; i < 4; i++) {
        int cx = cells[i] % FOREST_W;
        int cy = cells[i] / FOREST_W;
        forest[cy][cx] = placed[i];
    }
}

/** @brief Starts a new interstitial screen; follow with 1+ text_choice() calls. */
static void text_begin(const char *body)
{
    text_body = body;
    text_num_choices = 0;
    cursor = 0;
    state = STATE_INTERSTITIAL;
}

static void text_choice(TextAction action, const char *label)
{
    int i = text_num_choices++;
    text_choice_label[i] = label;
    text_choice_action[i] = action;
}

static void go_ende(int win, const char *text, bg_id_t bg)
{
    ende_is_win = win;
    ende_text = text;
    ende_bg = bg;
    state = STATE_ENDE;
    sfx_play(win ? SFX_PICKUP : SFX_EXPLOSION);
}

static void new_game(void)
{
    shuffle_stations();
    pos_x = PLAYER_START_X;
    pos_y = PLAYER_START_Y;
    dir_x = 0.0f; dir_y = 1.0f;
    /* plane must be dir rotated +90 deg (scaled to set the FOV) for
     * left/right to read correctly - see examples/raycaster/main.c's
     * dir=(1,0)/plane=(0,0.66) for the reference orientation this
     * mirrors. Getting the plane's sign backwards (as an earlier version
     * of this file did) mirrors the entire view left-right. */
    plane_x = -0.66f; plane_y = 0.0f;
    forest_entered = 0;
    sealed = 0;
    last_room_cell = -1;
    step_accum = 0.0f;
    turns = 0;
    has_ring = 0;
    ring_taken = 0;
    cheered_up = 0;
    state = STATE_FOREST;
}

/** @brief GameAPI init callback. */
static void game_init(void)
{
    w25q_init(); /* SPI1 is already running - game_run() set it up */
    sfx_init();
    rng_state ^= ((uint32_t)joystick_get_adc_x() << 16) ^ joystick_get_adc_y();
    music_init(death_waltz_melody);
    music_init_bass(death_waltz_bass);
    new_game();
    state = STATE_TITEL;
}

static void enter_forest(void)
{
    sfx_play(SFX_NOISE_SHORT);
    text_begin(txt_enter_forest);
    text_choice(ACTION_ENTER_FOREST, "...");
}

/** @brief Auto-triggered on first arrival in a station cell (see
 *         raycaster_update() below) - content is identical to
 *         examples/kobold's BTN-triggered interact(). */
static void interact_station(StationType s)
{
    switch (s) {
    case STATION_WALD:
    case STATION_HAUS:
    case STATION_MOOR:
        text_begin(station_text[s]);
        text_choice(ACTION_DISMISS, "WEITER");
        break;

    case STATION_BACH:
        if (!ring_taken) {
            text_begin(txt_bach_found);
            text_choice(ACTION_RING_WEAR, "RING ANLEGEN");
            text_choice(ACTION_RING_POCKET, "RING EINSTECKEN");
        } else {
            text_begin(txt_bach_empty);
            text_choice(ACTION_DISMISS, "WEITER");
        }
        break;

    case STATION_KOBOLD:
        /* WEITER always first (and so the default cursor position) - a
         * stray/accidental BTN press should never trigger ANGREIFEN. */
        text_begin(txt_kobold_story);
        text_choice(ACTION_DISMISS, "WEITER");
        if (!cheered_up)
            text_choice(ACTION_CHEER_UP, "AUFMUNTERN");
        if (has_ring && cheered_up)
            text_choice(ACTION_GIVE_RING, "RING GEBEN");
        text_choice(ACTION_ATTACK, "ANGREIFEN");
        break;

    default:
        break;
    }
}

static void confirm_choice(void)
{
    switch (text_choice_action[cursor]) {
    case ACTION_DISMISS:
        state = STATE_FOREST;
        break;

    case ACTION_ENTER_FOREST:
        sealed = 1;
        step_accum = 0.0f;
        turns = 0;
        state = STATE_FOREST;
        break;

    case ACTION_RING_WEAR:
        ring_taken = 1;
        go_ende(0, txt_lose_ring, BG_WALD);
        break;

    case ACTION_RING_POCKET:
        ring_taken = 1;
        has_ring = 1;
        sfx_play(SFX_PICKUP);
        state = STATE_FOREST;
        break;

    case ACTION_GIVE_RING:
        has_ring = 0;
        go_ende(1, txt_win, BG_WIN);
        break;

    case ACTION_CHEER_UP:
        cheered_up = 1;
        sfx_play(SFX_PICKUP);
        text_begin(txt_kobold_aufmuntern);
        text_choice(ACTION_DISMISS, "WEITER");
        break;

    case ACTION_ATTACK:
        go_ende(0, txt_lose_angriff, BG_ATTACK);
        break;
    }
}

/* ------------------------------------------------------------------- */
/* Movement + rendering (from examples/raycaster/main.c).                */
/* ------------------------------------------------------------------- */

static void rotate_vec(float *x, float *y, float cos_a, float sin_a)
{
    float ox = *x;
    *x = ox * cos_a - *y * sin_a;
    *y = ox * sin_a + *y * cos_a;
}

/** @brief GameAPI update callback. */
static void game_update(const JoystickState *js, uint32_t tick_ms)
{
    (void)tick_ms;

    switch (state) {
    case STATE_TITEL:
    case STATE_ENDE:
        if (js->pressed & JS_BTN)
            new_game();
        break;

    case STATE_INTERSTITIAL:
        move_cursor(js, text_num_choices);
        if (js->pressed & JS_BTN)
            confirm_choice();
        break;

    case STATE_FOREST: {
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
            float new_x = pos_x + dir_x * MOVE_SPEED * move_dir;
            float new_y = pos_y + dir_y * MOVE_SPEED * move_dir;

            /* Once sealed, the corridor back out of the room is closed -
             * "the hedges close behind you" - even though its cells are
             * still open floor in world_map[][]. */
            if (world_map[(int)pos_y][(int)new_x] == 0)
                pos_x = new_x;
            if (world_map[(int)new_y][(int)pos_x] == 0
                && !(sealed && new_y < (float)FOREST_ORIGIN_Y))
                pos_y = new_y;

            if (sealed) {
                step_accum += fabs_f(new_x - pos_x) + fabs_f(new_y - pos_y);
                if (step_accum >= 1.0f) {
                    step_accum -= 1.0f;
                    turns++;
                    sfx_play(SFX_MOVE);
                    if (turns >= MAX_TURNS)
                        go_ende(0, txt_lose_sleep, BG_WALD);
                }
            }
        }

        if (!forest_entered && pos_y >= (float)FOREST_ORIGIN_Y) {
            forest_entered = 1;
            enter_forest();
            break;  /* state is now STATE_INTERSTITIAL */
        }

        if (sealed) {
            int fx = (int)pos_x - FOREST_ORIGIN_X;
            int fy = (int)pos_y - FOREST_ORIGIN_Y;
            if (fx >= 0 && fx < FOREST_W && fy >= 0 && fy < FOREST_H) {
                int cell = fy * FOREST_W + fx;
                if (cell != last_room_cell) {
                    last_room_cell = cell;
                    StationType s = forest[fy][fx];
                    if (s != STATION_WALD)
                        interact_station(s);
                }
            }
        }
        break;
    }
    }
}

/**
 * @brief Procedural wall texture (see examples/raycaster/main.c's
 *        is_mortar() - identical algorithm, only the hedge pattern
 *        (type 2) is actually reachable on this map).
 */
static int is_mortar(int cell_type, int tex_x, int tex_y)
{
    if (cell_type == 1) {
        if (tex_y % 4 == 0)
            return 1;
        int course_offset = ((tex_y / 4) % 2) * 4;
        return ((tex_x + course_offset) % 8) == 0;
    }
    if (cell_type == 2)
        return (tex_x % 8 == 0) || (tex_y % 8 == 0);  /* trimmed hedge blocks */
    if (cell_type == 3)
        return (tex_x % 4 == 0);
    return 0;
}

/** @brief Casts one column's ray and draws its textured wall strip. */
static void cast_column(int x)
{
    float camera_x = 2.0f * x / (float)FB8_WIDTH - 1.0f;
    float ray_dir_x = dir_x + plane_x * camera_x;
    float ray_dir_y = dir_y + plane_y * camera_x;

    int map_x = (int)pos_x;
    int map_y = (int)pos_y;

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

    if (!hit)
        return;

    float perp_dist = (side == 0) ? (side_dist_x - delta_dist_x)
                                   : (side_dist_y - delta_dist_y);
    if (perp_dist < 0.01f)
        perp_dist = 0.01f;

    int line_height = (int)(FB8_HEIGHT / perp_dist);
    if (line_height < 1)
        line_height = 1;

    int draw_start_raw = -line_height / 2 + FB8_HEIGHT / 2;
    int draw_end_raw   =  line_height / 2 + FB8_HEIGHT / 2;

    int draw_start = draw_start_raw < 0 ? 0 : draw_start_raw;
    int draw_end   = draw_end_raw >= FB8_HEIGHT ? FB8_HEIGHT - 1 : draw_end_raw;

    float wall_x = (side == 0) ? (pos_y + perp_dist * ray_dir_y)
                                : (pos_x + perp_dist * ray_dir_x);
    wall_x -= (float)(int)wall_x;

    int tex_x = (int)(wall_x * 16.0f);
    if (tex_x < 0)  tex_x = 0;
    if (tex_x > 15) tex_x = 15;
    if ((side == 0 && ray_dir_x > 0.0f) || (side == 1 && ray_dir_y < 0.0f))
        tex_x = 15 - tex_x;

    float tex_step = 16.0f / (float)line_height;
    float tex_pos = (float)(draw_start - draw_start_raw) * tex_step;

    uint8_t fill_color = wall_colors[cell_type - 1][side];

    for (int y = draw_start; y <= draw_end; y++) {
        int tex_y = ((int)tex_pos) & 15;
        tex_pos += tex_step;

        uint8_t color = is_mortar(cell_type, tex_x, tex_y) ? MORTAR_COLOR : fill_color;
        framebuffer8[y * FB8_WIDTH + x] = color;
    }
}

/* ------------------------------------------------------------------- */
/* Rendering                                                             */
/* ------------------------------------------------------------------- */

static void draw_titel(void)
{
    draw_center(30,  GAME_TITLE, 5);
    draw_center(56,  GAME_TAGLINE, 15);
    draw_center(88,  GAME_PREMISE_1, 1);
    draw_center(102, GAME_PREMISE_2, 1);
    draw_center(116, GAME_PREMISE_3, 1);
    draw_center(150, "HOCH/RUNTER: LAUFEN", 2);
    draw_center(164, "LINKS/RECHTS: DREHEN", 2);
    draw_center(178, "KNOPF: BESTAETIGEN", 2);
    draw_center(210, "DRUECKE DEN KNOPF", 13);
}

static void draw_hud_forest(void)
{
    char line[48];
    int p = 0;
    char num[4];

    append(line, &p, "SCHRITTE ");
    utoa10((uint32_t)turns, num);
    append(line, &p, num);
    append(line, &p, "/");
    utoa10((uint32_t)MAX_TURNS, num);
    append(line, &p, num);
    if (has_ring)
        append(line, &p, "   RING BEI DIR");

    line[p] = 0;
    draw_text_shadow(4, HUD_Y, line, 1);
}

static void draw_ende(void)
{
    draw_center(8, ende_is_win ? "ENTKOMMEN!" : "GAME OVER",
                ende_is_win ? 13 : 1);
    draw_wrapped(TEXT_X, TEXT_Y0, TEXT_MAX_CHARS, TEXT_LINE_H, ende_text, 1);
    draw_center(HUD_Y, "DRUECKE DEN KNOPF FUER NEUSTART", 1);
}

/** @brief GameAPI draw callback. */
static void game_draw(void)
{
    if (state == STATE_TITEL) {
        draw_titel();
        return;
    }

    if (state == STATE_ENDE) {
        draw_scene_bg(ende_bg);
        draw_ende();
        return;
    }

    if (state == STATE_INTERSTITIAL) {
        /* Every interstitial (including txt_enter_forest, triggered right
         * as the player steps from the corridor onto the entrance cell)
         * fires while standing on a valid in-bounds room cell, so this
         * always resolves to that cell's photo - BG_PFAD is only a
         * defensive fallback that should never actually be reached. */
        int fx = (int)pos_x - FOREST_ORIGIN_X;
        int fy = (int)pos_y - FOREST_ORIGIN_Y;
        bg_id_t bg = (fx >= 0 && fx < FOREST_W && fy >= 0 && fy < FOREST_H)
                         ? (bg_id_t)forest[fy][fx]
                         : BG_PFAD;
        draw_scene_bg(bg);
        draw_wrapped(TEXT_X, TEXT_Y0, TEXT_MAX_CHARS, TEXT_LINE_H, text_body, 1);
        for (int i = 0; i < text_num_choices; i++)
            draw_choice_line(CHOICE_Y0 + i * CHOICE_LINE_H, text_choice_label[i], i == cursor);
        return;
    }

    /* STATE_FOREST: 3D walk. */
    fb8_fill_rect(0, 0, FB8_WIDTH, FB8_HEIGHT / 2, CEILING_COLOR);
    fb8_fill_rect(0, FB8_HEIGHT / 2, FB8_WIDTH, FB8_HEIGHT - FB8_HEIGHT / 2,
                  FLOOR_COLOR);

    for (int x = 0; x < FB8_WIDTH; x++)
        cast_column(x);

    if (sealed)
        draw_hud_forest();
}

static const GameAPI kobold3d_game = {
    .name   = "kobold3d",
    .init   = game_init,
    .update = game_update,
    .draw   = game_draw,
};

/** @brief Entry point: runs the game via the generic GameAPI loop. */
int main(void)
{
    game_run(&kobold3d_game, TICK_MS);
    return 0;
}
