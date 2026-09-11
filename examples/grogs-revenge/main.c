/**
 * @file main.c
 * @brief "Grogs Revenge" - a small arcade game built on the generic
 *        GameAPI, inspired by the 1984 C64 game "B.C. II: Grog's
 *        Revenge" (Thor rides across mountainsides collecting clams to
 *        pay Peter's toll, while Grog chases him and rocks/holes block
 *        the way). Simplified to one screen per mountain (no scrolling,
 *        no separate cave levels, no Tiredactyl) to fit this engine's
 *        tile-based room style, matching the dig-demo/Zauberschloss
 *        examples.
 *
 * All five joystick buttons are used: UP/DOWN/LEFT/RIGHT move Thor
 * (walking onto a clam collects it automatically, as in the original),
 * and BTN pays Peter's toll when Thor stands next to the post with
 * enough clams.
 *
 * The on-screen font only covers ASCII 32..127 (see font8x8.c), so all
 * German text here is written without umlauts/ß (UE/OE/AE/SS).
 */
#include "game.h"
#include "framebuffer8.h"
#include "sprite16.h"
#include "text.h"
#include "sfx.h"
#include "joystick.h"
#include "tiles.h"
#include "tileset16.h"
#include "mountains.h"

#define TICK_MS     150   /* movement/logic speed */
#define MESSAGE_MS  1800  /* how long a status message stays up */

#define DIR_UP     0
#define DIR_RIGHT  1
#define DIR_DOWN   2
#define DIR_LEFT   3

typedef enum {
    STATE_TITEL,
    STATE_SPIEL,
    STATE_SIEG,
    STATE_TOD,
} GameState;

/** @brief Thor, caveman on a stone unicycle. */
static const sprite16_t thor_sprite = {
    .px = {
        {0,0,0,0,0,0,8,8,8,8,0,0,0,0,0,0},
        {0,0,0,0,0,8,8,8,8,8,8,0,0,0,0,0},
        {0,0,0,0,8,8,1,8,8,1,8,8,0,0,0,0},
        {0,0,0,0,8,8,8,8,8,8,8,8,0,0,0,0},
        {0,0,0,0,9,9,9,9,9,9,9,9,0,0,0,0},
        {0,0,0,9,9,9,9,9,9,9,9,9,9,0,0,0},
        {0,0,9,9,9,9,9,9,9,9,9,9,9,9,0,0},
        {0,0,9,9,9,9,9,9,9,9,9,9,9,9,0,0},
        {0,0,0,9,9,9,9,9,9,9,9,9,9,0,0,0},
        {0,0,0,0,9,9,9,9,9,9,9,9,0,0,0,0},
        {0,0,0,0,0,9,9,9,9,9,9,0,0,0,0,0},
        {0,0,0,0,0,12,12,12,12,12,12,0,0,0,0,0},
        {0,0,0,0,12,12,11,11,11,11,12,12,0,0,0,0},
        {0,0,0,0,12,12,11,11,11,11,12,12,0,0,0,0},
        {0,0,0,0,0,12,12,12,12,12,12,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    }
};

/** @brief Grog, big and hairy, out for revenge. */
static const sprite16_t grog_sprite = {
    .px = {
        {0,0,0,2,2,0,0,0,0,0,0,2,2,0,0,0},
        {0,0,9,9,9,9,0,0,0,0,9,9,9,9,0,0},
        {0,9,9,9,9,9,9,0,0,9,9,9,9,9,9,0},
        {9,9,9,9,1,9,9,9,9,9,9,1,9,9,9,9},
        {9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9},
        {0,9,9,9,9,9,9,9,9,9,9,9,9,9,9,0},
        {0,9,9,9,9,9,9,9,9,9,9,9,9,9,9,0},
        {0,0,9,9,9,9,9,9,9,9,9,9,9,9,0,0},
        {0,0,9,9,9,9,9,9,9,9,9,9,9,9,0,0},
        {0,0,0,9,9,9,9,9,9,9,9,9,9,0,0,0},
        {0,0,0,0,9,9,9,9,9,9,9,9,0,0,0,0},
        {0,0,0,0,0,11,11,0,0,11,11,0,0,0,0,0},
        {0,0,0,0,0,11,11,0,0,11,11,0,0,0,0,0},
        {0,0,0,0,0,11,11,0,0,11,11,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    }
};

static GameState state;
static int mountain_index;
static int thor_x, thor_y;
static int grog_x, grog_y;
static int muscheln;
static int leben;

static char message[40];
static int32_t message_ms_left;

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

/** @brief Direction (DIR_UP..DIR_LEFT) to a (dx, dy) step. */
static void dir_delta(int dir, int *dx, int *dy)
{
    switch (dir) {
    case DIR_UP:    *dx = 0;  *dy = -1; break;
    case DIR_RIGHT: *dx = 1;  *dy = 0;  break;
    case DIR_DOWN:  *dx = 0;  *dy = 1;  break;
    default:        *dx = -1; *dy = 0;  break; /* DIR_LEFT */
    }
}

/** @brief Loads the given mountain and places Thor and Grog at their spawns. */
static void start_mountain(int index)
{
    mountain_index = index;
    mountain_load(mountain_index);

    thor_x = mountain_info.thor_start_x;
    thor_y = mountain_info.thor_start_y;
    grog_x = mountain_info.grog_start_x;
    grog_y = mountain_info.grog_start_y;

    muscheln = 0;
}

/** @brief Starts a fresh game from mountain 1 with full lives. */
static void neues_spiel(void)
{
    leben = 3;
    message[0] = 0;
    message_ms_left = 0;
    start_mountain(0);
}

/** @brief GameAPI init callback. */
static void grog_init(void)
{
    sfx_init();
    neues_spiel();
    state = STATE_TITEL;
}

/** @brief Sends Thor back to the mountain's start after a mishap, deducting a life. */
static void thor_mishap(const char *msg)
{
    sfx_play(SFX_EXPLOSION);
    leben--;
    if (leben <= 0) {
        state = STATE_TOD;
        return;
    }
    thor_x = mountain_info.thor_start_x;
    thor_y = mountain_info.thor_start_y;
    grog_x = mountain_info.grog_start_x;
    grog_y = mountain_info.grog_start_y;
    say(msg);
}

/** @brief Moves Thor one tile, handling clams, rocks/walls and holes. */
static void try_move(int dir)
{
    int dx, dy;
    dir_delta(dir, &dx, &dy);
    int nx = thor_x + dx;
    int ny = thor_y + dy;

    uint8_t t = get_tile(nx, ny);

    if (t == T_WALL || t == T_ROCK || t == T_TOLL)
        return;

    if (t == T_HOLE) {
        thor_mishap("IN EIN LOCH GEFALLEN!");
        return;
    }

    if (t == T_CLAM) {
        set_tile(nx, ny, T_EMPTY);
        muscheln++;
        sfx_play(SFX_PICKUP);
    }

    thor_x = nx;
    thor_y = ny;
}

/** @brief BTN: pays Peter's toll if Thor stands next to the post. */
static void do_action(void)
{
    int adjacent = (iabs(thor_x - mountain_info.toll_x) + iabs(thor_y - mountain_info.toll_y)) == 1;
    if (!adjacent)
        return;

    if (muscheln < mountain_info.clams_needed) {
        say("NICHT GENUG MUSCHELN!");
        sfx_play(SFX_NOISE_SHORT);
        return;
    }

    sfx_play(SFX_LASER);
    if (mountain_index + 1 >= MOUNTAIN_COUNT) {
        state = STATE_SIEG;
    } else {
        start_mountain(mountain_index + 1);
        say("NAECHSTER BERG!");
    }
}

/**
 * @brief Steps Grog one tile toward Thor (greedy chase, axis with the
 *        bigger gap first), blocked only by walls/rocks. Catching Thor
 *        costs a life.
 */
static void grog_update(void)
{
    int gx = thor_x - grog_x;
    int gy = thor_y - grog_y;
    int dx = (gx > 0) - (gx < 0);
    int dy = (gy > 0) - (gy < 0);
    int ax = iabs(gx);
    int ay = iabs(gy);

    if (ax >= ay) {
        uint8_t t = get_tile(grog_x + dx, grog_y);
        if (dx != 0 && t != T_WALL && t != T_ROCK)
            grog_x += dx;
        else if (dy != 0) {
            t = get_tile(grog_x, grog_y + dy);
            if (t != T_WALL && t != T_ROCK)
                grog_y += dy;
        }
    } else {
        uint8_t t = get_tile(grog_x, grog_y + dy);
        if (dy != 0 && t != T_WALL && t != T_ROCK)
            grog_y += dy;
        else if (dx != 0) {
            t = get_tile(grog_x + dx, grog_y);
            if (t != T_WALL && t != T_ROCK)
                grog_x += dx;
        }
    }

    if (grog_x == thor_x && grog_y == thor_y)
        thor_mishap("GROG HAT DICH ERWISCHT!");
}

/** @brief GameAPI update callback. */
static void grog_update_frame(const JoystickState *js, uint32_t tick_ms)
{
    if (message_ms_left > 0) {
        message_ms_left -= (int32_t)tick_ms;
        if (message_ms_left < 0)
            message_ms_left = 0;
    }

    switch (state) {
    case STATE_TITEL:
    case STATE_SIEG:
    case STATE_TOD:
        if (js->pressed & JS_BTN) {
            if (state != STATE_TITEL)
                neues_spiel();
            state = STATE_SPIEL;
        }
        break;

    case STATE_SPIEL:
        if (js->raw & JS_UP)         try_move(DIR_UP);
        else if (js->raw & JS_DOWN)  try_move(DIR_DOWN);
        else if (js->raw & JS_LEFT)  try_move(DIR_LEFT);
        else if (js->raw & JS_RIGHT) try_move(DIR_RIGHT);
        else if (js->pressed & JS_BTN) do_action();

        if (state == STATE_SPIEL)
            grog_update();
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
    draw_center(50,  "GROGS RACHE", 7);
    draw_center(90,  "SAMMLE MUSCHELN UND ZAHLE", 15);
    draw_center(106, "PETERS ZOLL, UM WEITERZUKOMMEN", 15);
    draw_center(140, "PFEIL-TASTEN: BEWEGEN", 1);
    draw_center(156, "KNOPF: ZOLL BEZAHLEN", 1);
    draw_center(190, "WEICHE GROG, LOECHERN UND FELSEN AUS!", 2);
    draw_center(216, "DRUECKE DEN KNOPF", 13);
}

static void draw_ende(int sieg)
{
    if (sieg) {
        draw_center(90,  "DU HAST GESIEGT!", 13);
        draw_center(110, "GROG KONNTE DICH NICHT AUFHALTEN.", 1);
    } else {
        draw_center(90,  "GAME OVER", 2);
        draw_center(110, "GROG HAT SEINE RACHE BEKOMMEN.", 1);
    }
    draw_center(150, "DRUECKE DEN KNOPF", 15);
}

static void draw_hud(void)
{
    char line[40];
    int p = 0;
    char num[4];

    if (message_ms_left > 0) {
        draw_text(4, FB8_HEIGHT - 14, message, 7);
        return;
    }

    append(line, &p, "BERG ");
    utoa10((uint32_t)(mountain_index + 1), num);
    append(line, &p, num);
    append(line, &p, "/");
    utoa10((uint32_t)MOUNTAIN_COUNT, num);
    append(line, &p, num);

    append(line, &p, "  MUSCHELN ");
    utoa10((uint32_t)muscheln, num);
    append(line, &p, num);
    append(line, &p, "/");
    utoa10((uint32_t)mountain_info.clams_needed, num);
    append(line, &p, num);

    append(line, &p, "  LEBEN ");
    utoa10((uint32_t)(leben < 0 ? 0 : leben), num);
    append(line, &p, num);

    line[p] = 0;
    draw_text(4, FB8_HEIGHT - 14, line, 1);
}

/** @brief GameAPI draw callback. */
static void grog_draw(void)
{
    if (state == STATE_TITEL) { draw_titel(); return; }
    if (state == STATE_SIEG) { draw_ende(1); return; }
    if (state == STATE_TOD) { draw_ende(0); return; }

    for (int y = 0; y < MAP_H; y++) {
        for (int x = 0; x < MAP_W; x++) {
            uint8_t t = get_tile(x, y);
            draw_sprite16(x * 16, y * 16, &tileset16[t]);
        }
    }

    draw_sprite16(grog_x * 16, grog_y * 16, &grog_sprite);
    draw_sprite16(thor_x * 16, thor_y * 16, &thor_sprite);

    draw_hud();
}

static const GameAPI grogs_revenge_game = {
    .name   = "grogs-revenge",
    .init   = grog_init,
    .update = grog_update_frame,
    .draw   = grog_draw,
};

/** @brief Entry point: runs Grogs Revenge via the generic GameAPI loop. */
int main(void)
{
    game_run(&grogs_revenge_game, TICK_MS);
    return 0;
}
