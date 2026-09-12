/**
 * @file main.c
 * @brief Story-neutral illustrated interactive-fiction gamebook template,
 *        extracted from examples/nebelkrone/ - copy this whole folder to
 *        start a new adventure. In the spirit of the solo gamebooks Das
 *        Schwarze Auge itself published: numbered choices instead of a
 *        text parser (this hardware has a joystick, not a keyboard), and
 *        single-die attribute checks - roll a d20 against one of three
 *        hero attributes (MUT/KLUGHEIT/GEWANDTHEIT) - as a compact
 *        stand-in for the full pen-and-paper ruleset.
 *
 * To build your own story on this template:
 *   - story.h/story.c: replace the placeholder Scene graph, Enemy
 *     table and GAME_TITLE/TAGLINE/PREMISE_* text with your own.
 *   - art.c/art.h/tileset16.c: the included tiles/illustrations are
 *     generic fantasy-adventure locations, usable as-is; extend or
 *     replace them (see tools/png2tileset.py to convert your own art).
 *   - track.c/track.h: swap in your own tune, or keep the default.
 *   - This file (main.c) normally doesn't need to change at all - it
 *     only reads story.h's data and constants.
 *
 * Every scene is a node in scenes[] (story.h/story.c): NODE_TEXT shows
 * an illustration plus a UP/DOWN+BTN choice menu; NODE_CHECK rolls a die
 * and branches automatically; NODE_COMBAT runs a small turn-based fight;
 * NODE_END_WIN/NODE_END_LOSE end the run. Illustrations are the same
 * tile-grid technique the other examples use for room maps (art.h/
 * art.c/tileset16.c), just composed as static backdrops. A short
 * background tune loops throughout, melody plus a root-note bass line
 * on the engine's second music voice (track.h, see music_init_bass()),
 * and sfx.h marks hits, pickups, danger and death.
 *
 * Controls: UP/DOWN move the menu cursor, BTN confirms.
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
#include "art.h"
#include "story.h"
#include "track.h"

#define TICK_MS 60

#define TEXT_X          4
#define TEXT_Y0         86
#define TEXT_LINE_H     11
#define TEXT_MAX_CHARS  38

#define CHOICE_X        4
#define CHOICE_Y0       168
#define CHOICE_LINE_H   12

#define COMBAT_STATUS_Y 168
#define COMBAT_CHOICE_Y0 180

#define HUD_Y           228

typedef enum {
    STATE_TITEL,
    STATE_SPIEL,
    STATE_ENDE,
} GameState;

typedef struct {
    int lp;
    int attr[ATTR_COUNT];
    uint32_t flags; /* bitmask of StoryFlag */
} Hero;

static GameState state;
static Hero hero;

static int current_scene;
static int cursor;
static int check_pending_target;
static int combat_enemy_lp;
static int ende_is_win;

static char display_text[256];

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

static void str_copy(char *dst, const char *src, int max)
{
    int i = 0;
    while (src[i] && i < max - 1) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = 0;
}

static uint32_t rng_next(void)
{
    rng_state = rng_state * 1664525u + 1013904223u;
    return rng_state;
}

/** @brief Rolls a d20 (1..20) - every attribute check in the game. */
static int d20(void)
{
    return (int)(rng_next() % 20) + 1;
}

/* ------------------------------------------------------------------- */
/* Text layout helpers                                                  */
/* ------------------------------------------------------------------- */

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
        draw_text(x, line_y, buf, color);

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
    draw_text(x, y, s, color);
}

/** @brief Draws one menu line, highlighting it with a ">" if selected. */
static void draw_choice_line(int y, const char *text, int selected)
{
    char buf[40];
    int p = 0;
    append(buf, &p, selected ? "> " : "  ");
    append(buf, &p, text);
    buf[p] = 0;
    draw_text(CHOICE_X, y, buf, selected ? 7 : 12);
}

/* ------------------------------------------------------------------- */
/* Story engine                                                         */
/* ------------------------------------------------------------------- */

static void goto_scene(int id);

/**
 * @brief Applies damage to the hero; on lethal damage, jumps straight to
 *        the death ending with the given reason as its flavor text.
 * @return 1 if the hero died (caller must stop processing this tick),
 *         else 0.
 */
static int apply_damage(int dmg, const char *reason)
{
    hero.lp -= dmg;
    if (hero.lp <= 0) {
        hero.lp = 0;
        str_copy(display_text, reason ? reason : "Deine Kraft verlaesst dich...", sizeof(display_text));
        goto_scene(SCENE_END_LOSE);
        return 1;
    }
    if (dmg > 0)
        sfx_play(SFX_NOISE_SHORT);
    return 0;
}

/** @brief Runs a scene's on-entry effects (item grants, penalties, rolls). */
static void handle_enter(void)
{
    const Scene *s = &scenes[current_scene];

    if (s->grant_flag >= 0)
        hero.flags |= (1u << s->grant_flag);

    if (s->penalty_req_flag >= 0 && !(hero.flags & (1u << s->penalty_req_flag))) {
        if (apply_damage((int)s->penalty_damage, s->penalty_reason))
            return;
    }

    if (s->enter_sfx != SFX_NONE)
        sfx_play(s->enter_sfx);

    switch (s->kind) {
    case NODE_TEXT:
    case NODE_COMBAT:
        str_copy(display_text, s->text, sizeof(display_text));
        if (s->kind == NODE_COMBAT)
            combat_enemy_lp = enemies[s->enemy_id].lp;
        break;

    case NODE_CHECK: {
        int roll = d20();
        int passed = roll <= hero.attr[s->check_attr];
        int p = 0;
        append(display_text, &p, s->text);
        append(display_text, &p, "\n");
        append(display_text, &p, attr_name[s->check_attr]);
        append(display_text, &p, "-PROBE: WURF ");
        char num[4];
        utoa10((uint32_t)roll, num);
        append(display_text, &p, num);
        append(display_text, &p, passed ? " - GESCHAFFT!" : " - MISSLUNGEN!");
        display_text[p] = 0;

        if (!passed && s->check_fail_damage > 0) {
            if (apply_damage((int)s->check_fail_damage, s->check_fail_reason))
                return;
        }
        check_pending_target = passed ? s->check_pass_target : s->check_fail_target;
        break;
    }

    case NODE_END_WIN:
        str_copy(display_text, s->text, sizeof(display_text));
        break;

    case NODE_END_LOSE:
        /* display_text was already set by apply_damage()'s reason,
         * unless this scene was reached directly - then fall back. */
        if (display_text[0] == 0)
            str_copy(display_text, s->text, sizeof(display_text));
        break;
    }

    if (s->kind == NODE_END_WIN || s->kind == NODE_END_LOSE) {
        state = STATE_ENDE;
        ende_is_win = (s->kind == NODE_END_WIN);
    }
}

static void goto_scene(int id)
{
    current_scene = id;
    cursor = 0;
    handle_enter();
}

static void story_start(void)
{
    hero.lp = HERO_LP_START;
    hero.attr[ATTR_MUT] = HERO_MUT_START;
    hero.attr[ATTR_KLUGHEIT] = HERO_KLUGHEIT_START;
    hero.attr[ATTR_GEWANDTHEIT] = HERO_GEWANDTHEIT_START;
    hero.flags = 0;
    display_text[0] = 0;
    state = STATE_SPIEL;
    goto_scene(SCENE_INTRO);
}

/** @brief GameAPI init callback. */
static void game_init(void)
{
    sfx_init();
    rng_state ^= ((uint32_t)joystick_get_adc_x() << 16) ^ joystick_get_adc_y();
    music_init(template_track);
    music_init_bass(template_bass);
    story_start();
    state = STATE_TITEL;
}

/** @brief Resolves one combat round for the chosen action (see below). */
static void do_combat_action(int action)
{
    const Scene *s = &scenes[current_scene];
    const Enemy *e = &enemies[s->enemy_id];
    char num[4];
    int p = 0;

    if (action == 1) { /* FLIEHEN */
        goto_scene(s->combat_flee_target);
        return;
    }

    if (action == 2) { /* use the found heal item, see FLAG_HEAL_ITEM in story.h */
        hero.lp += HEAL_ITEM_AMOUNT;
        if (hero.lp > HERO_LP_MAX) hero.lp = HERO_LP_MAX;
        hero.flags &= ~(1u << FLAG_HEAL_ITEM);
        sfx_play(SFX_PICKUP);
        append(display_text, &p, HEAL_ITEM_USE_MESSAGE);
    } else { /* ATTACKE */
        int roll = d20();
        if (roll <= hero.attr[ATTR_GEWANDTHEIT]) {
            int dmg = 3 + (int)(rng_next() % 4);
            combat_enemy_lp -= dmg;
            sfx_play(SFX_LASER);
            append(display_text, &p, "Du triffst den ");
            append(display_text, &p, e->name);
            append(display_text, &p, " (-");
            utoa10((uint32_t)dmg, num);
            append(display_text, &p, num);
            append(display_text, &p, " LP).");
        } else {
            sfx_play(SFX_NOISE_SHORT);
            append(display_text, &p, "Du verfehlst den Gegner.");
        }
    }

    if (combat_enemy_lp <= 0) {
        append(display_text, &p, " Er ist besiegt!");
        display_text[p] = 0;
        goto_scene(s->combat_win_target);
        return;
    }

    int eroll = d20();
    if (eroll <= e->hit_chance) {
        int edmg = e->dmg_min + (int)(rng_next() % (uint32_t)(e->dmg_max - e->dmg_min + 1));
        append(display_text, &p, " Der ");
        append(display_text, &p, e->name);
        append(display_text, &p, " trifft dich (-");
        utoa10((uint32_t)edmg, num);
        append(display_text, &p, num);
        append(display_text, &p, " LP).");
        display_text[p] = 0;

        /* Built from the enemy's own name, so any Enemy table entry
         * gets a sensible death reason for free - no per-enemy text
         * to maintain. */
        char death_msg[64];
        int dp = 0;
        append(death_msg, &dp, "Der ");
        append(death_msg, &dp, e->name);
        append(death_msg, &dp, " ueberwaeltigt dich im Kampf.");
        death_msg[dp] = 0;

        if (apply_damage(edmg, death_msg))
            return;
    } else {
        append(display_text, &p, " Der ");
        append(display_text, &p, e->name);
        append(display_text, &p, " verfehlt dich.");
        display_text[p] = 0;
    }
}

/** @brief UP/DOWN cursor movement shared by NODE_TEXT and NODE_COMBAT menus. */
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

/** @brief GameAPI update callback. */
static void game_update(const JoystickState *js, uint32_t tick_ms)
{
    (void)tick_ms;

    switch (state) {
    case STATE_TITEL:
    case STATE_ENDE:
        if (js->pressed & JS_BTN)
            story_start();
        break;

    case STATE_SPIEL: {
        const Scene *s = &scenes[current_scene];
        switch (s->kind) {
        case NODE_TEXT:
            move_cursor(js, s->num_choices);
            if (js->pressed & JS_BTN)
                goto_scene(s->choice_target[cursor]);
            break;

        case NODE_CHECK:
            if (js->pressed & JS_BTN)
                goto_scene(check_pending_target);
            break;

        case NODE_COMBAT: {
            int has_heal_item = (hero.flags & (1u << FLAG_HEAL_ITEM)) != 0;
            int count = has_heal_item ? 3 : 2;
            if (cursor >= count) cursor = count - 1;
            move_cursor(js, count);
            if (js->pressed & JS_BTN)
                do_combat_action(cursor);
            break;
        }

        case NODE_END_WIN:
        case NODE_END_LOSE:
            break; /* goto_scene() already switched to STATE_ENDE */
        }
        break;
    }
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
    draw_center(150, "HOCH/RUNTER: AUSWAEHLEN", 2);
    draw_center(164, "KNOPF: BESTAETIGEN", 2);
    draw_center(210, "DRUECKE DEN KNOPF", 13);
}

static void draw_hud(void)
{
    char line[48];
    int p = 0;
    char num[4];

    append(line, &p, "LP ");
    utoa10((uint32_t)hero.lp, num);
    append(line, &p, num);
    append(line, &p, "/");
    utoa10((uint32_t)HERO_LP_MAX, num);
    append(line, &p, num);

    append(line, &p, "  MUT ");
    utoa10((uint32_t)hero.attr[ATTR_MUT], num);
    append(line, &p, num);

    append(line, &p, "  KL ");
    utoa10((uint32_t)hero.attr[ATTR_KLUGHEIT], num);
    append(line, &p, num);

    append(line, &p, "  GE ");
    utoa10((uint32_t)hero.attr[ATTR_GEWANDTHEIT], num);
    append(line, &p, num);

    line[p] = 0;
    draw_text(4, HUD_Y, line, 1);
}

static void draw_combat_status(void)
{
    const Scene *s = &scenes[current_scene];
    const Enemy *e = &enemies[s->enemy_id];
    char line[48];
    int p = 0;
    char num[4];

    append(line, &p, "DU ");
    utoa10((uint32_t)hero.lp, num);
    append(line, &p, num);
    append(line, &p, " LP   ");
    append(line, &p, e->name);
    append(line, &p, " ");
    utoa10((uint32_t)(combat_enemy_lp < 0 ? 0 : combat_enemy_lp), num);
    append(line, &p, num);
    append(line, &p, " LP");

    line[p] = 0;
    draw_text(TEXT_X, COMBAT_STATUS_Y, line, 10);
}

static void draw_ende(void)
{
    draw_center(8, ende_is_win ? "SIEG!" : "GAME OVER", ende_is_win ? 13 : 2);
    draw_center(HUD_Y, "DRUECKE DEN KNOPF FUER NEUSTART", 15);
}

/** @brief GameAPI draw callback. */
static void game_draw(void)
{
    if (state == STATE_TITEL) {
        draw_titel();
        return;
    }

    const Scene *s = &scenes[current_scene];
    draw_scene_bg(s->bg);
    draw_wrapped(TEXT_X, TEXT_Y0, TEXT_MAX_CHARS, TEXT_LINE_H, display_text, 15);

    if (state == STATE_ENDE) {
        draw_ende();
        return;
    }

    switch (s->kind) {
    case NODE_TEXT:
        for (int i = 0; i < s->num_choices; i++)
            draw_choice_line(CHOICE_Y0 + i * CHOICE_LINE_H, s->choice_text[i], i == cursor);
        break;

    case NODE_CHECK:
        draw_choice_line(CHOICE_Y0, "WEITER", 1);
        break;

    case NODE_COMBAT: {
        draw_combat_status();
        int has_heal_item = (hero.flags & (1u << FLAG_HEAL_ITEM)) != 0;
        static const char *const labels[3] = { "ATTACKE", "FLIEHEN", HEAL_ITEM_LABEL };
        int count = has_heal_item ? 3 : 2;
        for (int i = 0; i < count; i++)
            draw_choice_line(COMBAT_CHOICE_Y0 + i * CHOICE_LINE_H, labels[i], i == cursor);
        break;
    }

    default:
        break;
    }

    draw_hud();
}

static const GameAPI template_game = {
    .name   = "gamebook-template",
    .init   = game_init,
    .update = game_update,
    .draw   = game_draw,
};

/** @brief Entry point: runs the gamebook via the generic GameAPI loop. */
int main(void)
{
    game_run(&template_game, TICK_MS);
    return 0;
}
