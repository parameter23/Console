/**
 * @file main.c
 * @brief Proof-of-concept real side-scroller on the generic GameAPI.
 *
 * Unlike dig-demo/Zauberschloss/Grogs Revenge, the hero does not move
 * in whole-tile steps on a fixed screen: player_x is a pixel position
 * in a level that is wider than the screen (level.h), the camera
 * follows it in single pixels, and draw_sprite16() is called at
 * whatever sub-tile screen offset that produces - draw_sprite16()
 * already accepts arbitrary pixel coordinates, so nothing in the
 * engine had to change to support this, only how a game uses it.
 *
 * The tick is also much shorter than the other examples (see TICK_MS)
 * so movement/physics update close to every frame instead of once
 * every 150 ms - that's what makes the scroll read as smooth motion
 * rather than as grid steps.
 *
 * Controls: LEFT/RIGHT run, UP or BTN jumps. Touching a spike or
 * falling into a pit costs a life; collect coins; reach the flag to win.
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
#include "tiles.h"
#include "tileset16.h"
#include "level.h"

#define TICK_MS        30   /* short tick -> physics updates near every frame */
#define MESSAGE_MS     1500

#define GRAVITY        1
#define JUMP_VY        (-13)
#define MOVE_SPEED     3
#define MAX_FALL_VY    14

#define COIN_ROW       7
#define NUM_DRAW_COLS  ((FB8_WIDTH / 16) + 1)
#define TOTAL_COINS    13

typedef enum {
    STATE_TITEL,
    STATE_SPIEL,
    STATE_SIEG,
    STATE_TOD,
} GameState;

/** @brief The hero, mid-stride silhouette (works for both running and jumping). */
static const sprite16_t hero_sprite = {
    .px = {
        {0,0,0,0,0,0,8,8,8,8,0,0,0,0,0,0},
        {0,0,0,0,0,8,8,8,8,8,8,0,0,0,0,0},
        {0,0,0,0,8,8,1,8,8,1,8,8,0,0,0,0},
        {0,0,0,0,8,8,8,8,8,8,8,8,0,0,0,0},
        {0,0,0,0,6,6,6,6,6,6,6,6,0,0,0,0},
        {0,0,0,6,6,6,6,6,6,6,6,6,6,0,0,0},
        {0,0,6,6,6,6,6,6,6,6,6,6,6,6,0,0},
        {0,0,6,6,6,6,6,6,6,6,6,6,6,6,0,0},
        {0,0,0,6,6,6,6,6,6,6,6,6,6,0,0,0},
        {0,0,0,0,6,6,6,6,6,6,6,6,0,0,0,0},
        {0,0,0,0,0,9,9,0,0,9,9,0,0,0,0,0},
        {0,0,0,9,9,9,0,0,0,0,9,9,9,0,0,0},
        {0,0,9,9,0,0,0,0,0,0,0,0,9,9,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    }
};

static GameState state;

static int player_x;   /* world pixels */
static int player_y;   /* screen pixels (no vertical scroll) */
static int vy;
static int on_ground;

static int leben;
static int muenzen;

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

/** @brief Places the hero standing on the ground at the level's start. */
static void respawn(void)
{
    player_x = 16;
    player_y = (ground_row[1] - 1) * 16;
    vy = 0;
    on_ground = 1;
}

/** @brief Starts a fresh run: rebuilds the level, resets score and lives. */
static void neues_spiel(void)
{
    level_build();
    leben = 3;
    muenzen = 0;
    message[0] = 0;
    message_ms_left = 0;
    respawn();
}

/** @brief GameAPI init callback. */
static void scroller_init(void)
{
    sfx_init();
    neues_spiel();
    state = STATE_TITEL;
}

/** @brief Costs a life and either ends the game or respawns the hero. */
static void mishap(const char *msg)
{
    sfx_play(SFX_EXPLOSION);
    leben--;
    if (leben <= 0) {
        state = STATE_TOD;
        return;
    }
    respawn();
    say(msg);
}

/** @brief Runs one physics/logic tick: input, gravity, collision, pickups. */
static void step_physics(const JoystickState *js)
{
    if (js->raw & JS_LEFT)  player_x -= MOVE_SPEED;
    if (js->raw & JS_RIGHT) player_x += MOVE_SPEED;
    if (player_x < 0) player_x = 0;
    if (player_x > (LEVEL_W - 1) * 16) player_x = (LEVEL_W - 1) * 16;

    if ((js->pressed & (JS_UP | JS_BTN)) && on_ground) {
        vy = JUMP_VY;
        on_ground = 0;
        sfx_play(SFX_BEEP);
    }

    vy += GRAVITY;
    if (vy > MAX_FALL_VY) vy = MAX_FALL_VY;
    player_y += vy;

    int col = (player_x + 8) / 16;
    if (col < 0) col = 0;
    if (col >= LEVEL_W) col = LEVEL_W - 1;

    on_ground = 0;
    if (ground_row[col] < MAP_H) {
        int ground_y = ground_row[col] * 16;
        if (player_y + 16 >= ground_y && vy >= 0) {
            player_y = ground_y - 16;
            vy = 0;
            on_ground = 1;
        }
    }

    if (player_y > MAP_H * 16) {
        mishap("IN EINE SCHLUCHT GEFALLEN!");
        return;
    }

    if (on_ground && obstacle_at[col]) {
        mishap("IN EINEN STACHEL GELAUFEN!");
        return;
    }

    int row_top = player_y / 16;
    int row_bottom = (player_y + 15) / 16;
    if (coin_at[col] && COIN_ROW >= row_top && COIN_ROW <= row_bottom) {
        coin_at[col] = 0;
        muenzen++;
        sfx_play(SFX_PICKUP);
    }

    if (col >= GOAL_COL) {
        state = STATE_SIEG;
        sfx_play(SFX_LASER);
    }
}

/** @brief GameAPI update callback. */
static void scroller_update(const JoystickState *js, uint32_t tick_ms)
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
        step_physics(js);
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
    draw_center(50,  "SEITENSCROLLER DEMO", 7);
    draw_center(90,  "ECHTES PIXELWEISES SCROLLING", 15);
    draw_center(106, "AUF DERSELBEN GAME-API", 15);
    draw_center(140, "LINKS/RECHTS: LAUFEN", 1);
    draw_center(156, "HOCH ODER KNOPF: SPRINGEN", 1);
    draw_center(190, "SAMMLE MUENZEN, WEICHE STACHELN", 2);
    draw_center(206, "UND SCHLUCHTEN AUS - ERREICHE DIE FAHNE!", 2);
}

static void draw_ende(int sieg)
{
    if (sieg) {
        draw_center(90,  "ZIEL ERREICHT!", 13);
        draw_center(110, "DU HAST ES GESCHAFFT.", 1);
    } else {
        draw_center(90,  "GAME OVER", 2);
        draw_center(110, "VERSUCH ES NOCH EINMAL.", 1);
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

    append(line, &p, "MUENZEN ");
    utoa10((uint32_t)muenzen, num);
    append(line, &p, num);
    append(line, &p, "/");
    utoa10(TOTAL_COINS, num);
    append(line, &p, num);

    append(line, &p, "  LEBEN ");
    utoa10((uint32_t)(leben < 0 ? 0 : leben), num);
    append(line, &p, num);

    line[p] = 0;
    draw_text(4, FB8_HEIGHT - 14, line, 1);
}

/** @brief GameAPI draw callback: draws the level with a pixel-precise camera. */
static void scroller_draw(void)
{
    if (state == STATE_TITEL) { draw_titel(); return; }
    if (state == STATE_SIEG) { draw_ende(1); return; }
    if (state == STATE_TOD) { draw_ende(0); return; }

    int camera_x = player_x - 100;
    if (camera_x < 0) camera_x = 0;
    if (camera_x > LEVEL_W * 16 - FB8_WIDTH) camera_x = LEVEL_W * 16 - FB8_WIDTH;

    int first_col = camera_x / 16;
    int frac = camera_x - first_col * 16;

    for (int i = 0; i < NUM_DRAW_COLS; i++) {
        int world_col = first_col + i;
        if (world_col >= LEVEL_W)
            break;
        int screen_x = i * 16 - frac;

        uint8_t gr = ground_row[world_col];
        for (int row = 0; row < MAP_H; row++) {
            tile_id_t t = (row < gr) ? T_SKY : (row == gr ? T_GRASS : T_DIRT);
            draw_sprite16(screen_x, row * 16, &tileset16[t]);
        }

        if (obstacle_at[world_col] && gr < MAP_H)
            draw_sprite16(screen_x, (gr - 1) * 16, &tileset16[T_SPIKE]);

        if (coin_at[world_col])
            draw_sprite16(screen_x, COIN_ROW * 16, &tileset16[T_COIN]);

        if (world_col == GOAL_COL && gr < MAP_H)
            draw_sprite16(screen_x, (gr - 1) * 16, &tileset16[T_GOAL]);
    }

    draw_sprite16(player_x - camera_x, player_y, &hero_sprite);

    draw_hud();
}

static const GameAPI scroller_game = {
    .name   = "scroller-demo",
    .init   = scroller_init,
    .update = scroller_update,
    .draw   = scroller_draw,
};

/** @brief Entry point: runs the scroller demo via the generic GameAPI loop. */
int main(void)
{
    game_run(&scroller_game, TICK_MS);
    return 0;
}
