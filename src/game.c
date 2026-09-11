#include "game.h"
#include "clock.h"
#include "st7789.h"
#include "framebuffer8.h"

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

    while (1) {
        JoystickState js = joystick_update();
        uint32_t now = millis();

        if (now - last_tick >= tick_ms) {
            last_tick = now;
            if (game->update)
                game->update(&js, tick_ms);
        }

        fb8_clear(0);
        if (game->draw)
            game->draw();
        fb8_flush_dma();
    }
}
