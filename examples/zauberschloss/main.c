/**
 * @file main.c
 * @brief "Zauberschloss" - a small Zelda-style RPG built on the generic
 *        GameAPI, inspired by the 1984 C64 text adventure of the same
 *        name (steal the crown from the wizard, then escape the castle
 *        with it). All five joystick buttons are used: UP/DOWN/LEFT/
 *        RIGHT move the hero, and BTN performs whatever action fits
 *        the tile the hero is facing (take an item, unlock a door,
 *        fight a guard, use the gate) - the joystick equivalent of the
 *        original's two-word text commands.
 *
 * The on-screen font only covers ASCII 32..127 (see font8x8.c), so all
 * German text here is written without umlauts/ß (UE/OE/AE/SS), the way
 * many 8-bit games of the era handled it.
 */
#include "game.h"
#include "framebuffer8.h"
#include "sprite16.h"
#include "text.h"
#include "sfx.h"
#include "joystick.h"
#include "tiles.h"
#include "tileset16.h"
#include "rooms.h"

#define TICK_MS           150   /* movement/logic speed */
#define MESSAGE_MS        1800  /* how long a status message stays up */
#define START_X           9
#define START_Y           7
#define THRONSAAL_ENTRY_X 1     /* just inside Thronsaal's door, for the "caught" push-back */
#define THRONSAAL_ENTRY_Y 7

typedef enum {
    STATE_TITEL,
    STATE_SPIEL,
    STATE_ENDE_SIEG,
    STATE_ENDE_TOD,
} GameState;

/** @brief The hero (blue tunic, to tell him apart from the grey guard). */
static const sprite16_t player_sprite = {
    .px = {
        {0,0,0,0,0,0,6,6,6,6,0,0,0,0,0,0},
        {0,0,0,0,0,6,6,6,6,6,6,0,0,0,0,0},
        {0,0,0,0,6,6,1,6,6,1,6,6,0,0,0,0},
        {0,0,0,0,6,6,6,6,6,6,6,6,0,0,0,0},
        {0,0,0,0,6,6,6,6,6,6,6,6,0,0,0,0},
        {0,0,0,6,6,6,6,6,6,6,6,6,6,0,0,0},
        {0,0,6,6,6,6,6,6,6,6,6,6,6,6,0,0},
        {0,0,6,6,6,6,6,6,6,6,6,6,6,6,0,0},
        {0,0,0,6,6,6,6,6,6,6,6,6,6,0,0,0},
        {0,0,0,0,6,6,6,6,6,6,6,6,0,0,0,0},
        {0,0,0,0,0,6,6,6,6,6,6,0,0,0,0,0},
        {0,0,0,0,0,9,9,0,0,9,9,0,0,0,0,0},
        {0,0,0,0,0,9,9,0,0,9,9,0,0,0,0,0},
        {0,0,0,0,0,9,9,0,0,9,9,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    }
};

static GameState state;

static int room_id;
static int player_x, player_y;
static int facing;

static int hat_schluessel;
static int hat_schwert;
static int hat_krone;
static int leben;

static int wizard_aktiv;   /* wizard is present/drawn (room_id == ROOM_THRONSAAL) */
static int wizard_jagt;    /* wizard is actively chasing the player */
static int wizard_x, wizard_y;

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

/** @brief Switches the active room and (re)places the wizard if needed. */
static void switch_room(int new_room_id)
{
    room_id = new_room_id;
    room_enter(room_id);

    wizard_aktiv = (room_id == ROOM_THRONSAAL);
    if (wizard_aktiv) {
        wizard_x = wizard_spawn_x;
        wizard_y = wizard_spawn_y;
        wizard_jagt = 0;
    }
}

/** @brief Starts a fresh game: rebuilds every room and resets the hero. */
static void neues_spiel(void)
{
    rooms_init();

    player_x = START_X;
    player_y = START_Y;
    facing = DIR_DOWN;

    hat_schluessel = 0;
    hat_schwert = 0;
    hat_krone = 0;
    leben = 3;

    wizard_aktiv = 0;
    wizard_jagt = 0;

    message[0] = 0;
    message_ms_left = 0;

    room_id = ROOM_EINGANG;
    room_enter(room_id);
}

/** @brief GameAPI init callback. */
static void zauberschloss_init(void)
{
    sfx_init();
    neues_spiel();
    state = STATE_TITEL;
}

/**
 * @brief Moves the hero one tile in dir if the target tile is open
 *        floor, or crosses into the neighboring room if the hero walks
 *        off the room's edge through a connected opening.
 */
static void try_move(int dir)
{
    int dx, dy;
    dir_delta(dir, &dx, &dy);
    int nx = player_x + dx;
    int ny = player_y + dy;

    if (nx < 0 || nx >= MAP_W || ny < 0 || ny >= MAP_H) {
        int target = room_neighbor(room_id, dir);
        if (target < 0)
            return; /* solid outer wall, no passage here */

        switch_room(target);

        if (nx < 0)       player_x = MAP_W - 1;
        else if (nx >= MAP_W) player_x = 0;
        else               player_x = nx;

        if (ny < 0)       player_y = MAP_H - 1;
        else if (ny >= MAP_H) player_y = 0;
        else               player_y = ny;

        return;
    }

    if (get_tile(nx, ny) == T_EMPTY) {
        player_x = nx;
        player_y = ny;
    }
    /* Anything else (wall, door, item, guard, gate) blocks movement -
     * it's resolved with the action button instead. */
}

/** @brief BTN: resolves whatever the hero is facing. */
static void do_action(void)
{
    int dx, dy;
    dir_delta(facing, &dx, &dy);
    int tx = player_x + dx;
    int ty = player_y + dy;
    uint8_t t = get_tile(tx, ty);

    switch (t) {
    case T_KEY:
        hat_schluessel = 1;
        set_tile(tx, ty, T_EMPTY);
        say("SCHLUESSEL GEFUNDEN!");
        sfx_play(SFX_PICKUP);
        break;

    case T_SWORD:
        hat_schwert = 1;
        set_tile(tx, ty, T_EMPTY);
        say("SCHWERT GEFUNDEN!");
        sfx_play(SFX_PICKUP);
        break;

    case T_CROWN:
        hat_krone = 1;
        set_tile(tx, ty, T_EMPTY);
        wizard_jagt = 1;
        say("DU HAST DIE KRONE GESTOHLEN!");
        sfx_play(SFX_LASER);
        break;

    case T_DOOR_LOCKED:
        if (hat_schluessel) {
            set_tile(tx, ty, T_EMPTY);
            say("TUER AUFGESCHLOSSEN.");
            sfx_play(SFX_BEEP);
        } else {
            say("VERSCHLOSSEN - DU BRAUCHST EINEN SCHLUESSEL.");
            sfx_play(SFX_NOISE_SHORT);
        }
        break;

    case T_GUARD:
        if (hat_schwert) {
            set_tile(tx, ty, T_EMPTY);
            say("DIE WACHE IST BESIEGT!");
            sfx_play(SFX_EXPLOSION);
        } else {
            say("DU BRAUCHST EIN SCHWERT!");
            sfx_play(SFX_NOISE_SHORT);
        }
        break;

    case T_EXIT:
        if (hat_krone) {
            state = STATE_ENDE_SIEG;
            sfx_play(SFX_LASER);
        } else {
            say("ERST DIE KRONE STEHLEN!");
            sfx_play(SFX_BEEP);
        }
        break;

    default:
        break;
    }
}

/**
 * @brief Steps the wizard one tile toward the hero (simple greedy
 *        chase, axis with the bigger gap first). Only runs while
 *        wizard_jagt is set (i.e. after the crown was taken and the
 *        hero is still in Thronsaal).
 */
static void wizard_update(void)
{
    if (!wizard_jagt)
        return;

    int gx = player_x - wizard_x;
    int gy = player_y - wizard_y;
    int dx = (gx > 0) - (gx < 0);
    int dy = (gy > 0) - (gy < 0);
    int ax = (gx < 0) ? -gx : gx;
    int ay = (gy < 0) ? -gy : gy;

    if (ax >= ay) {
        if (dx != 0 && get_tile(wizard_x + dx, wizard_y) != T_WALL)
            wizard_x += dx;
        else if (dy != 0 && get_tile(wizard_x, wizard_y + dy) != T_WALL)
            wizard_y += dy;
    } else {
        if (dy != 0 && get_tile(wizard_x, wizard_y + dy) != T_WALL)
            wizard_y += dy;
        else if (dx != 0 && get_tile(wizard_x + dx, wizard_y) != T_WALL)
            wizard_x += dx;
    }

    if (wizard_x == player_x && wizard_y == player_y) {
        hat_krone = 0;
        wizard_jagt = 0;
        set_tile(wizard_x, wizard_y, T_CROWN);

        player_x = THRONSAAL_ENTRY_X; /* thrown back to the doorway */
        player_y = THRONSAAL_ENTRY_Y;

        sfx_play(SFX_EXPLOSION);
        leben--;
        if (leben <= 0)
            state = STATE_ENDE_TOD;
        else
            say("DER ZAUBERER HAT DICH ERWISCHT!");
    }
}

/** @brief GameAPI update callback. */
static void zauberschloss_update(const JoystickState *js, uint32_t tick_ms)
{
    if (message_ms_left > 0) {
        message_ms_left -= (int32_t)tick_ms;
        if (message_ms_left < 0)
            message_ms_left = 0;
    }

    switch (state) {
    case STATE_TITEL:
    case STATE_ENDE_SIEG:
    case STATE_ENDE_TOD:
        if (js->pressed & JS_BTN) {
            if (state != STATE_TITEL)
                neues_spiel();
            state = STATE_SPIEL;
        }
        break;

    case STATE_SPIEL:
        if (js->raw & JS_UP)         { facing = DIR_UP;    try_move(DIR_UP); }
        else if (js->raw & JS_DOWN)  { facing = DIR_DOWN;  try_move(DIR_DOWN); }
        else if (js->raw & JS_LEFT)  { facing = DIR_LEFT;  try_move(DIR_LEFT); }
        else if (js->raw & JS_RIGHT) { facing = DIR_RIGHT; try_move(DIR_RIGHT); }
        else if (js->pressed & JS_BTN) do_action();

        wizard_update();
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
    draw_center(60,  "ZAUBERSCHLOSS", 7);
    draw_center(100, "STIEHL DIE KRONE DES ZAUBERERS", 15);
    draw_center(116, "UND ENTKOMME AUS DEM SCHLOSS", 15);
    draw_center(150, "PFEIL-TASTEN: BEWEGEN", 1);
    draw_center(166, "KNOPF: NEHMEN / OEFFNEN / KAEMPFEN", 1);
    draw_center(200, "DRUECKE DEN KNOPF", 13);
}

static void draw_ende(int sieg)
{
    if (sieg) {
        draw_center(90,  "DU HAST GESIEGT!", 13);
        draw_center(110, "DAS ZAUBERSCHLOSS IST BEZWUNGEN.", 1);
    } else {
        draw_center(90,  "GAME OVER", 2);
        draw_center(110, "DER ZAUBERER HAT DICH GEFANGEN.", 1);
    }
    draw_center(150, "DRUECKE DEN KNOPF", 15);
}

static void draw_hud(void)
{
    char line[40];
    int p = 0;

    if (message_ms_left > 0) {
        draw_text(4, FB8_HEIGHT - 14, message, 7);
        return;
    }

    append(line, &p, "SCHL:");
    append(line, &p, hat_schluessel ? "J " : "N ");
    append(line, &p, "SCHW:");
    append(line, &p, hat_schwert ? "J " : "N ");
    append(line, &p, "LEBEN:");
    char num[4];
    utoa10((uint32_t)(leben < 0 ? 0 : leben), num);
    append(line, &p, num);
    line[p] = 0;

    draw_text(4, FB8_HEIGHT - 14, line, 1);
}

/** @brief GameAPI draw callback. */
static void zauberschloss_draw(void)
{
    if (state == STATE_TITEL) { draw_titel(); return; }
    if (state == STATE_ENDE_SIEG) { draw_ende(1); return; }
    if (state == STATE_ENDE_TOD) { draw_ende(0); return; }

    for (int y = 0; y < MAP_H; y++) {
        for (int x = 0; x < MAP_W; x++) {
            uint8_t t = get_tile(x, y);
            draw_sprite16(x * 16, y * 16, &tileset16[t]);
        }
    }

    if (wizard_aktiv)
        draw_sprite16(wizard_x * 16, wizard_y * 16, &tileset16[T_WIZARD]);

    draw_sprite16(player_x * 16, player_y * 16, &player_sprite);

    draw_hud();
}

static const GameAPI zauberschloss_game = {
    .name   = "zauberschloss",
    .init   = zauberschloss_init,
    .update = zauberschloss_update,
    .draw   = zauberschloss_draw,
};

/** @brief Entry point: runs Zauberschloss via the generic GameAPI loop. */
int main(void)
{
    game_run(&zauberschloss_game, TICK_MS);
    return 0;
}
