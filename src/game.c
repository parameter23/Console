/**
 * @file game.c
 * @brief Implementation of the generic GameAPI main loop (see game.h).
 */
#include "game.h"
#include "clock.h"
#include "ili9488.h"
#include "framebuffer8.h"

/** @brief See game_run() in the header for the full contract. */
void game_run(const GameAPI *game, uint32_t tick_ms)
{
    clock_setup();
    systick_setup();

    ili9488_init();
    fb8_init_palette();

    joystick_init();

    if (game->init)
        game->init();

    uint32_t last_tick = millis();

    while (1) {
        JoystickState js = joystick_update();
        uint32_t now = millis();

        /* Fixed-timestep catch-up: each game->update() call represents
         * exactly tick_ms of elapsed time (games rely on that, e.g.
         * countdown timers subtracting tick_ms directly). A single
         * "if" here would silently cap the logic rate at the render
         * frame rate whenever a flush takes longer than tick_ms -
         * catching up with a bounded loop instead keeps game speed
         * tied to wall-clock time regardless of how long drawing
         * takes. The iteration cap is just spiral-of-death protection
         * (e.g. after a debugger pause), not a normal code path. */
        int catchup = 0;
        while (now - last_tick >= tick_ms && catchup++ < 5) {
            last_tick += tick_ms;
            if (game->update)
                game->update(&js, tick_ms);

            /* js was polled once for this whole catch-up burst - raw
             * (held) state is still valid for every simulated tick, but
             * pressed/released/repeat are one-shot edges. Without
             * clearing them, a single button press gets replayed into
             * every extra catch-up update (e.g. double-stepping a menu
             * cursor) whenever a frame runs behind. */
            js.pressed = 0;
            js.released = 0;
            js.repeat = 0;
        }

        fb8_clear(0);
        if (game->draw)
            game->draw();
        fb8_flush_dma();
    }
}
