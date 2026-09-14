/**
 * @file st7789.h
 * @brief Driver for the ST7789 SPI TFT controller (320x240, RGB565).
 *
 * CS is PB0, not PA4: the BlackPill board's onboard SOI8 footprint for
 * a W25Qxx SPI flash chip is hard-wired to PA4 (CS)/PA5 (SCK)/PA6
 * (MISO)/PA7 (MOSI) - the same SPI1 bus this display uses. Sharing
 * SCK/MISO/MOSI between two SPI slaves is fine, but each device needs
 * its own CS, so the display's CS lives on PB0 to leave PA4 free for
 * the flash chip (see w25q128.h).
 */
#ifndef DISPLAY_ST7789_H
#define DISPLAY_ST7789_H

#include <stdint.h>
#include <libopencm3/stm32/gpio.h>

/* Display control pins: CS = PB0, DC = PA3, RST = PA2 */
#define ST7789_CS_PORT   GPIOB
#define ST7789_CS_PIN    GPIO0
#define ST7789_DC_PORT   GPIOA
#define ST7789_DC_PIN    GPIO3
#define ST7789_RST_PORT  GPIOA
#define ST7789_RST_PIN   GPIO2

#define CS_LOW()   gpio_clear(ST7789_CS_PORT, ST7789_CS_PIN)
#define CS_HIGH()  gpio_set(ST7789_CS_PORT, ST7789_CS_PIN)
#define DC_DATA()  gpio_set(ST7789_DC_PORT, ST7789_DC_PIN)
#define DC_CMD()   gpio_clear(ST7789_DC_PORT, ST7789_DC_PIN)

/**
 * @brief Configures SPI1 and the control GPIOs, resets the panel and
 *        runs the ST7789 init sequence (landscape MADCTL, RGB565,
 *        inversion on, sleep out, display on). Call once.
 */
void st7789_init(void);

/**
 * @brief Fills an axis-aligned rectangle on the panel with one RGB565 color.
 * @param x, y  Top-left corner.
 * @param w, h  Width and height in pixels.
 * @param color RGB565 color.
 */
void st7789_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);

/**
 * @brief Sets one pixel on the panel directly (no framebuffer involved).
 * @param x, y  Pixel coordinates.
 * @param color RGB565 color.
 */
void st7789_draw_pixel(uint16_t x, uint16_t y, uint16_t color);

/**
 * @brief Sets the panel's addressing window (column/row range) that
 *        subsequent pixel data writes will fill.
 * @param x0, y0 Top-left corner (inclusive).
 * @param x1, y1 Bottom-right corner (inclusive).
 */
void st7789_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);

/**
 * @brief Sends a raw command byte (DC low) to the panel.
 * @param cmd Command byte.
 */
void st7789_cmd(uint8_t cmd);

/**
 * @brief Sends a raw data byte (DC high) to the panel.
 * @param data Data byte.
 */
void st7789_data(uint8_t data);

#endif
