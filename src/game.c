/**
 * @file game.c
 * @brief Implementation of the generic GameAPI main loop (see game.h).
 */
#include "game.h"
#include "clock.h"
#include "st7789.h"
#include "framebuffer8.h"

/** @brief See game_run() in the header for the full contract. */
void game_run(const GameAPI *game, uint32_t tick_ms)
{
    clock_setup();
    systick_setup();

    st7789_init();
    fb8_init_palette();

    joystick_init();

    if (game->init)
        game->init();

    uint32_t last_tick = millis();

    /* One-shot joystick edges (pressed/released/repeat), accumulated
     * across polls that don't get delivered to game->update() this
     * iteration (see below) - OR'd in, not overwritten, and cleared
     * only once actually handed to an update() call. */
    uint8_t pending_pressed = 0, pending_released = 0, pending_repeat = 0;

    while (1) {
        JoystickState polled = joystick_update();
        pending_pressed  |= polled.pressed;
        pending_released |= polled.released;
        pending_repeat   |= polled.repeat;

        uint32_t now = millis();

        /* Fixed-timestep catch-up: each game->update() call represents
         * exactly tick_ms of elapsed time (games rely on that, e.g.
         * countdown timers subtracting tick_ms directly). A single
         * "if" here would silently cap the logic rate at the render
         * frame rate whenever a flush takes longer than tick_ms -
         * catching up with a bounded loop instead keeps game speed
         * tied to wall-clock time regardless of how long drawing
         * takes. The iteration cap is just spiral-of-death protection
         * (e.g. after a debugger pause), not a normal code path.
         *
         * joystick_update() above runs once per *outer* iteration -
         * as fast as draw+flush allows (tens of ms on this panel) -
         * while this inner loop only runs game->update() once tick_ms
         * has actually elapsed. Whenever tick_ms is longer than that
         * (e.g. a 150ms grid-movement tick against a much faster
         * flush), most outer iterations poll the joystick without
         * ever calling update() that same iteration. A one-shot edge
         * bit polled on such an iteration used to be silently
         * discarded - and since joystick_update()'s internal last_raw
         * had already advanced past it, that specific press could
         * never produce a pressed=1 again, so a menu often needed
         * several presses before one happened to land on an iteration
         * where update() actually ran. Accumulating edges into
         * pending_pressed/released/repeat above (instead of reading
         * them fresh off a single poll) fixes that: every edge is
         * kept until it's actually delivered. */
        int catchup = 0;
        while (now - last_tick >= tick_ms && catchup++ < 5) {
            last_tick += tick_ms;

            JoystickState js;
            js.raw      = polled.raw;
            js.pressed  = pending_pressed;
            js.released = pending_released;
            js.repeat   = pending_repeat;
            pending_pressed = 0;
            pending_released = 0;
            pending_repeat = 0;

            if (game->update)
                game->update(&js, tick_ms);
        }

        fb8_clear(0);
        if (game->draw)
            game->draw();
        fb8_flush_dma();
    }
}
