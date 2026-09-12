/**
 * @file game.h
 * @brief Reusable GameAPI: hardware bring-up + main loop for any game
 *        built on this engine.
 *
 * A game implements the three GameAPI callbacks (init/update/draw) and
 * hands the struct to game_run(), which owns all hardware bring-up
 * (clock, display, framebuffer, joystick) and the main loop. Individual
 * games never call clock_setup()/ili9488_init()/etc. themselves.
 */
#ifndef GAME_H
#define GAME_H

#include <stdint.h>
#include "joystick.h"

/**
 * @brief Callback table implemented by a game and passed to game_run().
 */
typedef struct {
    const char *name;   /**< Human-readable name of the game. */

    /** @brief Called once, after all hardware is initialised. */
    void (*init)(void);

    /**
     * @brief Called at a fixed cadence with the current joystick state.
     *        Game logic belongs here.
     * @param js       Current joystick state (never NULL).
     * @param tick_ms  The tick period passed to game_run(), in ms.
     */
    void (*update)(const JoystickState *js, uint32_t tick_ms);

    /**
     * @brief Called once per main-loop iteration, after update().
     *
     * The framebuffer is already cleared to color 0 when this runs;
     * draw() only needs to paint content - game_run() flushes it to the
     * display afterwards.
     */
    void (*draw)(void);
} GameAPI;

/**
 * @brief Brings up the hardware and runs a game's main loop. Never returns.
 *
 * Initialises the clock, SysTick, display, framebuffer and joystick,
 * calls game->init() once, then loops forever: poll the joystick, call
 * game->update() every tick_ms, call game->draw() every iteration, and
 * flush the framebuffer to the display.
 *
 * @param game     The game to run (its init/update/draw callbacks may
 *                 be NULL to skip that step).
 * @param tick_ms  Fixed cadence, in milliseconds, at which update() is
 *                 called.
 */
void game_run(const GameAPI *game, uint32_t tick_ms);

#endif
