/**
 * @file main.c
 * @brief "Der Kobold" - a small illustrated interactive-fiction game
 *        built from gamebook-template. Unlike a normal gamebook-
 *        template story, this one doesn't fit the template's linear
 *        Scene-graph engine (NODE_TEXT/CHECK/COMBAT/END with fixed
 *        choice_target links): it's a randomized 3x3 forest explored
 *        freely with the joystick, closer in spirit to a room-graph
 *        game. So this file owns the actual game logic (map
 *        generation, movement, turn counter, inventory) instead of
 *        just reading a static scenes[] array - see story.h's header
 *        comment for the full reasoning.
 *
 * Setup: three path tiles lead up to a forest that can only be entered
 * from the last one. Once inside, the hedges close - the 3x3 grid's
 * outer edge (including the way back out) is impassable from then on,
 * which falls out for free from simply clamping movement to the grid:
 * no separate "sealed" flag needed. Four stations (Altes Haus, Kobold,
 * Moor, Bach) are shuffled into four of the eight non-entrance cells
 * every new game (shuffle_stations()) - the rest are plain forest.
 *
 * The kobold is a cursed woman; finding and handing her the ring from
 * the Bach breaks the curse and opens the way out (win). Wearing the
 * ring instead turns the player into a kobold themselves (lose), and
 * taking more than MAX_TURNS steps through the forest means collapsing
 * from exhaustion (lose).
 *
 * Controls: arrow keys move (UP/DOWN on the path, all four directions
 * in the forest); BTN looks around / interacts at the current forest
 * cell, and confirms choices on a text screen.
 *
 * The on-screen font only covers ASCII 32..127 (see font8x8.c), so all
 * German text here is written without umlauts/ß (UE/OE/AE/SS).
 */
#include "game.h"
#include "framebuffer8.h"
#include "text.h"
#include "sfx.h"
#include "music.h"
#include "joystick.h"
#include "art.h"
#include "story.h"
#include "track.h"
#include "w25q128.h"

#define TICK_MS 150

#define TEXT_X          4
#define TEXT_Y0         86
#define TEXT_LINE_H     11
#define TEXT_MAX_CHARS  38

#define CHOICE_X        4
#define CHOICE_Y0       168
#define CHOICE_LINE_H   12

#define HINT_Y          150
#define HUD_Y           228

#define ENTRANCE_X 1
#define ENTRANCE_Y 0

typedef enum {
    STATE_TITEL,
    STATE_PATH,
    STATE_FOREST,
    STATE_TEXT,   /* a text screen with 1+ choices, see text_begin()/text_choice() */
    STATE_ENDE,
} GameState;

/** @brief What confirming a STATE_TEXT screen's selected choice does. */
typedef enum {
    ACTION_DISMISS,      /* just close the text screen */
    ACTION_ENTER_FOREST, /* close the forest-warning text, start exploring */
    ACTION_RING_WEAR,    /* "RING ANLEGEN" at the Bach */
    ACTION_RING_POCKET,  /* "RING EINSTECKEN" at the Bach */
    ACTION_GIVE_RING,    /* "RING GEBEN" at the Kobold */
    ACTION_CHEER_UP,     /* "AUFMUNTERN" at the Kobold */
    ACTION_ATTACK,       /* "ANGREIFEN" at the Kobold */
} TextAction;

#define MAX_TEXT_CHOICES 4

static GameState state;

static int path_pos; /* 0..2, the three tiles in front of the forest */

static StationType forest[FOREST_H][FOREST_W];
static int fx, fy;   /* player's forest cell */
static int turns;    /* steps taken inside the forest, see MAX_TURNS */

static int has_ring;   /* carrying the ring, not yet given away */
static int ring_taken; /* the Bach's ring already resolved (worn or pocketed) */
static int cheered_up; /* comforted the kobold at least once - required
                         * before she'll accept the ring */

static int ende_is_win;
static const char *ende_text;
static bg_id_t ende_bg;

/* The active STATE_TEXT screen. */
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

/* ------------------------------------------------------------------- */
/* Text layout helpers                                                  */
/* ------------------------------------------------------------------- */

/** @brief Draws text with a full 1px black outline for legibility over
 *         the full-screen photo backgrounds (draw_text() only paints
 *         "on" glyph pixels, leaving everything else - including
 *         whatever busy image content sits behind the text - alone). A
 *         single-corner drop shadow wasn't enough contrast against some
 *         of the photos - outlining every side is much more robust
 *         regardless of what's behind any given character. */
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

/** @brief UP/DOWN cursor movement for a STATE_TEXT choice menu. */
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
/* Game logic                                                           */
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

/**
 * @brief Resets the 3x3 forest to plain forest, then places the four
 *        stations into four of the eight non-entrance cells at random -
 *        a fresh layout every new game.
 */
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

/** @brief Starts a new text screen; follow with 1+ calls to text_choice(). */
static void text_begin(const char *body)
{
    text_body = body;
    text_num_choices = 0;
    cursor = 0;
    state = STATE_TEXT;
}

/** @brief Appends one choice (up to MAX_TEXT_CHOICES) to the open text screen. */
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
    path_pos = 0;
    fx = ENTRANCE_X;
    fy = ENTRANCE_Y;
    turns = 0;
    has_ring = 0;
    ring_taken = 0;
    cheered_up = 0;
    state = STATE_PATH;
}

/** @brief GameAPI init callback. */
static void game_init(void)
{
    w25q_init(); /* SPI1 is already running - game_run() set it up */
    sfx_init();
    rng_state ^= ((uint32_t)joystick_get_adc_x() << 16) ^ joystick_get_adc_y();
    music_init(template_track);
    music_init_bass(template_bass);
    new_game();
    state = STATE_TITEL;
}

static void enter_forest(void)
{
    fx = ENTRANCE_X;
    fy = ENTRANCE_Y;
    turns = 0;
    sfx_play(SFX_NOISE_SHORT);
    text_begin(txt_enter_forest);
    text_choice(ACTION_ENTER_FOREST, "...");
}

static void try_move_path(int dy)
{
    if (dy > 0) {
        if (path_pos < 2) {
            path_pos++;
            sfx_play(SFX_MOVE);
        } else {
            enter_forest();
        }
    } else if (dy < 0 && path_pos > 0) {
        path_pos--;
        sfx_play(SFX_MOVE);
    }
}

static int forest_in_bounds(int x, int y)
{
    return x >= 0 && x < FOREST_W && y >= 0 && y < FOREST_H;
}

/**
 * @brief Attempts one forest step. The grid's outer edge (0..2 on both
 *        axes) is always impassable, including the entrance cell's
 *        edge back to the path - that's what "the hedges close behind
 *        you" amounts to: there is no special sealed-shut state to
 *        track, the boundary was always a wall from the player's side.
 */
static void try_move_forest(int dx, int dy)
{
    int nx = fx + dx, ny = fy + dy;
    if (!forest_in_bounds(nx, ny))
        return;

    fx = nx;
    fy = ny;
    sfx_play(SFX_MOVE);

    turns++;
    if (turns >= MAX_TURNS)
        go_ende(0, txt_lose_sleep, BG_WALD);
}

/** @brief BTN at the player's current forest cell: look around / interact. */
static void interact(void)
{
    StationType s = forest[fy][fx];

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
        /* WEITER always first (and so the default cursor position) -
         * a stray/accidental BTN press should never trigger ANGREIFEN,
         * which ends the game. */
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

    case STATE_PATH:
        if (js->raw & JS_UP)        try_move_path(1);
        else if (js->raw & JS_DOWN) try_move_path(-1);
        break;

    case STATE_FOREST:
        /* BTN is checked independently of movement, not chained onto
         * the same else-if: on this hardware, pressing BTN can nudge
         * the stick enough to also report a direction bit that same
         * tick, and movement taking priority was silently dropping the
         * BTN press whenever that happened. */
        if (js->raw & JS_UP)         try_move_forest(0, -1);
        else if (js->raw & JS_DOWN)  try_move_forest(0, 1);
        else if (js->raw & JS_LEFT)  try_move_forest(-1, 0);
        else if (js->raw & JS_RIGHT) try_move_forest(1, 0);
        if (js->pressed & JS_BTN)
            interact();
        break;

    case STATE_TEXT:
        move_cursor(js, text_num_choices);
        if (js->pressed & JS_BTN)
            confirm_choice();
        break;
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
    draw_center(150, "PFEILTASTEN: BEWEGEN", 2);
    draw_center(164, "KNOPF: UMSEHEN / BESTAETIGEN", 2);
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

/* Top-right corner, clear of the hint/HUD text at the bottom and the
 * story/choice text block starting at TEXT_Y0. */
#define COMPASS_CX 296
#define COMPASS_CY 20
#define COMPASS_D  14

/**
 * @brief Draws an arrow for each of the four directions the player can
 *        currently step in the forest. The full-screen photo
 *        backgrounds give no other visual cue for this - unlike a
 *        tile-based room map, there is no wall/path graphic to read.
 */
static void draw_compass(void)
{
    if (forest_in_bounds(fx, fy - 1))
        draw_text_shadow(COMPASS_CX - 4, COMPASS_CY - COMPASS_D, "^", 1);
    if (forest_in_bounds(fx, fy + 1))
        draw_text_shadow(COMPASS_CX - 4, COMPASS_CY + COMPASS_D, "v", 1);
    if (forest_in_bounds(fx - 1, fy))
        draw_text_shadow(COMPASS_CX - COMPASS_D - 4, COMPASS_CY, "<", 1);
    if (forest_in_bounds(fx + 1, fy))
        draw_text_shadow(COMPASS_CX + COMPASS_D - 4, COMPASS_CY, ">", 1);
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

    if (state == STATE_PATH) {
        draw_scene_bg(BG_PFAD);
        draw_center(HINT_Y, txt_hint_path, 1);
        return;
    }

    if (state == STATE_FOREST) {
        draw_scene_bg((bg_id_t)forest[fy][fx]);
        draw_center(HINT_Y, txt_hint_forest, 1);
        draw_hud_forest();
        draw_compass();
        return;
    }

    /* STATE_TEXT: keep showing the forest cell the player is standing
     * on behind the text, so the screen doesn't flash to something
     * unrelated for a one-line "WEITER" prompt. */
    draw_scene_bg((bg_id_t)forest[fy][fx]);
    draw_wrapped(TEXT_X, TEXT_Y0, TEXT_MAX_CHARS, TEXT_LINE_H, text_body, 1);
    for (int i = 0; i < text_num_choices; i++)
        draw_choice_line(CHOICE_Y0 + i * CHOICE_LINE_H, text_choice_label[i], i == cursor);
}

static const GameAPI kobold_game = {
    .name   = "kobold",
    .init   = game_init,
    .update = game_update,
    .draw   = game_draw,
};

/** @brief Entry point: runs the game via the generic GameAPI loop. */
int main(void)
{
    game_run(&kobold_game, TICK_MS);
    return 0;
}
