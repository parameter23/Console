#ifndef DISPLAY_ST7789_H
#define DISPLAY_ST7789_H

#include <stdint.h>
#include <libopencm3/stm32/gpio.h>

/* Display control pins: CS = PA4, DC = PA3, RST = PA2 */
#define ST7789_CS_PORT   GPIOA
#define ST7789_CS_PIN    GPIO4
#define ST7789_DC_PORT   GPIOA
#define ST7789_DC_PIN    GPIO3
#define ST7789_RST_PORT  GPIOA
#define ST7789_RST_PIN   GPIO2

#define CS_LOW()   gpio_clear(ST7789_CS_PORT, ST7789_CS_PIN)
#define CS_HIGH()  gpio_set(ST7789_CS_PORT, ST7789_CS_PIN)
#define DC_DATA()  gpio_set(ST7789_DC_PORT, ST7789_DC_PIN)
#define DC_CMD()   gpio_clear(ST7789_DC_PORT, ST7789_DC_PIN)

void st7789_init(void);
void st7789_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
void st7789_draw_pixel(uint16_t x, uint16_t y, uint16_t color);
void st7789_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
void st7789_cmd(uint8_t cmd);
void st7789_data(uint8_t data);

#endif
