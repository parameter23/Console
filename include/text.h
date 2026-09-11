#ifndef GRAPHICS_TEXT_H
#define GRAPHICS_TEXT_H

#include <stdint.h>

void draw_char(int x, int y, char c, uint8_t color);
void draw_text(int x, int y, const char *s, uint8_t color);

#endif
