#include "sprite16.h"
#include "framebuffer8.h"

void draw_sprite16(int x, int y, const sprite16_t *spr)
{
    for (int sy = 0; sy < 16; sy++) {
        int py = y + sy;
        if (py < 0 || py >= FB8_HEIGHT)
            continue;

        for (int sx = 0; sx < 16; sx++) {
            int px = x + sx;
            if (px < 0 || px >= FB8_WIDTH)
                continue;

            uint8_t c = spr->px[sy][sx];
            if (c == 0)
                continue; /* transparent */

            fb8_set_pixel(px, py, c);
        }
    }
}
