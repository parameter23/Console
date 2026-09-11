#ifndef GAME_H
#define GAME_H

#include <stdint.h>
#include "joystick.h"

/*
 * Minimal game-development API on top of the hardware/engine layer
 * (clock, st7789, framebuffer8, sprite16, tiles, joystick, sound,
 * text). A game implements this struct and hands it to game_run(),
 * which owns hardware bring-up and the main loop - individual games
 * never call clock_setup()/st7789_init()/etc. themselves.
 */
typedef struct {
    const char *name;

    /* Called once, after all hardware is initialised. */
    void (*init)(void);

    /* Called at a fixed cadence (see game_run()'s tick_ms) with the
     * current joystick state. Game logic belongs here. */
    void (*update)(const JoystickState *js, uint32_t tick_ms);

    /* Called once per main-loop iteration, after update(). The
     * framebuffer is already cleared to color 0; draw() only paints
     * content - game_run() flushes it to the display afterwards. */
    void (*draw)(void);
} GameAPI;

/*
 * Brings up the hardware (clock, display, framebuffer, joystick), runs
 * game->init() once, then loops forever: poll the joystick, call
 * game->update() every tick_ms, call game->draw() every iteration and
 * flush the framebuffer. Never returns.
 */
void game_run(const GameAPI *game, uint32_t tick_ms);

#endif
