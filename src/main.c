#include "game.h"
#include "framebuffer8.h"
#include "sprite16.h"
#include "text.h"
#include "sfx.h"
#include "music.h"
#include "sound.h"
#include "joystick.h"
#include "tiles.h"
#include "tileset16.h"
#include "levels.h"

/*
 * Demo game built on the generic GameAPI (game.h/game.c). Shows how a
 * game plugs into game_run(): implement init()/update()/draw() and
 * hand them to game_run() in main(). No falling-rock physics here -
 * this is a walk-dig-collect demo to exercise the tile map, sprites,
 * sound and text modules end to end.
 */

#define TICK_MS  150  /* grid-move speed */

static const sprite16_t player_sprite = {
    .px = {
        {0,0,0,0,0,0,2,2,2,2,0,0,0,0,0,0},
        {0,0,0,0,0,2,2,2,2,2,2,0,0,0,0,0},
        {0,0,0,0,2,2,1,2,2,1,2,2,0,0,0,0},
        {0,0,0,0,2,2,2,2,2,2,2,2,0,0,0,0},
        {0,0,0,0,2,2,2,2,2,2,2,2,0,0,0,0},
        {0,0,0,2,2,2,2,2,2,2,2,2,2,0,0,0},
        {0,0,2,2,2,2,2,2,2,2,2,2,2,2,0,0},
        {0,0,2,2,2,2,2,2,2,2,2,2,2,2,0,0},
        {0,0,0,2,2,2,2,2,2,2,2,2,2,0,0,0},
        {0,0,0,0,2,2,2,2,2,2,2,2,0,0,0,0},
        {0,0,0,0,0,2,2,2,2,2,2,0,0,0,0,0},
        {0,0,0,0,0,2,2,0,0,2,2,0,0,0,0,0},
        {0,0,0,0,0,2,2,0,0,2,2,0,0,0,0,0},
        {0,0,0,0,0,2,2,0,0,2,2,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    }
};

static int player_x, player_y;
static uint32_t score;

/* Minimal unsigned-int-to-decimal-string - no sprintf under -nostdlib. */
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

static void draw_hud(void)
{
    char line[32];
    char num[12];
    int p = 0;

    append(line, &p, "SCORE ");
    utoa10(score, num);
    append(line, &p, num);
    line[p] = 0;
    draw_text(4, FB8_HEIGHT - 10, line, 1);
}

static void demo_init(void)
{
    sfx_init();
    music_init(boulder_track);

    level_load(0);

    player_x = 1;
    player_y = 1;
    for (int y = 0; y < MAP_H; y++) {
        for (int x = 0; x < MAP_W; x++) {
            if (get_tile(x, y) == T_PLAYER_SPAWN) {
                player_x = x;
                player_y = y;
                set_tile(x, y, T_EMPTY);
            }
        }
    }

    score = 0;
}

static void try_move(int dx, int dy)
{
    int tx = player_x + dx;
    int ty = player_y + dy;
    uint8_t t = get_tile(tx, ty);

    if (t == T_STEEL || t == T_ROCK)
        return;

    if (t == T_DIAMOND) {
        score++;
        bd_sound_play(SND_DIAMOND);
    } else {
        bd_sound_play(SND_STEP);
    }

    set_tile(tx, ty, T_EMPTY);
    player_x = tx;
    player_y = ty;
}

static void demo_update(const JoystickState *js, uint32_t tick_ms)
{
    (void)tick_ms;

    if (js->raw & JS_UP)         try_move(0, -1);
    else if (js->raw & JS_DOWN)  try_move(0, 1);
    else if (js->raw & JS_LEFT)  try_move(-1, 0);
    else if (js->raw & JS_RIGHT) try_move(1, 0);
}

static void demo_draw(void)
{
    bd_sound_update();

    for (int y = 0; y < MAP_H; y++) {
        for (int x = 0; x < MAP_W; x++) {
            uint8_t t = get_tile(x, y);
            draw_sprite16(x * 16, y * 16, &tileset16[t]);
        }
    }

    draw_sprite16(player_x * 16, player_y * 16, &player_sprite);

    draw_hud();
}

static const GameAPI demo_game = {
    .name   = "dig-demo",
    .init   = demo_init,
    .update = demo_update,
    .draw   = demo_draw,
};

int main(void)
{
    game_run(&demo_game, TICK_MS);
    return 0;
}
