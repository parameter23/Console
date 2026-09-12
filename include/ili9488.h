/**
 * @file ili9488.h
 * @brief Driver for the ILI9488/ILI9486 SPI TFT controller (480x320, RGB565).
 *
 * Replaces the ST7789 driver this engine used to target. Same control
 * pins, command set (0x2A/0x2B/0x2C addressing is MIPI-DBI-compatible
 * across these controllers) and 16bpp RGB565 pixel format as ST7789 -
 * just a different init sequence and a bigger panel.
 *
 * NOTE: cheap "3.5in SPI TFT" modules sold as ILI9488 quite often
 * actually carry an ILI9486 (there's no reliable way to tell from the
 * outside, and this project has no wired-up MISO/SDO to read the
 * controller's ID back). Sending the 18bpp/RGB666 (3 bytes/pixel) mode
 * that real ILI9488 panels prefer over SPI to a board that is actually
 * an ILI9486 desyncs the controller's internal write-pointer partway
 * through the frame (classic symptom: the top of the image bleeds back
 * in near the bottom of the screen). Plain RGB565 is what ILI9486
 * expects and what most ILI9488 modules accept fine too, so that's
 * what this driver uses.
 *
 *  - The panel's native orientation is 320(W)x480(H); MADCTL here picks
 *    a landscape rotation to present it as 480x320. Whether that also
 *    needs MX/MY mirror bits flipped depends on how the panel is
 *    physically mounted/wired - check on real hardware and adjust
 *    ILI9488_MADCTL below if the image comes up mirrored/rotated.
 *
 * CS is PB0, not PA4: the BlackPill board's onboard SOI8 footprint for
 * a W25Qxx SPI flash chip is hard-wired to PA4 (CS)/PA5 (SCK)/PA6
 * (MISO)/PA7 (MOSI) - the same SPI1 bus this display uses. Sharing
 * SCK/MISO/MOSI between two SPI slaves is fine, but each device needs
 * its own CS, so the display's CS moved off PA4 to leave it free for
 * the flash chip (see w25q128.h).
 */
#ifndef DISPLAY_ILI9488_H
#define DISPLAY_ILI9488_H

#include <stdint.h>
#include <libopencm3/stm32/gpio.h>

/* Display control pins: CS = PB0, DC = PA3, RST = PA2 */
#define ILI9488_CS_PORT   GPIOB
#define ILI9488_CS_PIN    GPIO0
#define ILI9488_DC_PORT   GPIOA
#define ILI9488_DC_PIN    GPIO3
#define ILI9488_RST_PORT  GPIOA
#define ILI9488_RST_PIN   GPIO2

#define CS_LOW()   gpio_clear(ILI9488_CS_PORT, ILI9488_CS_PIN)
#define CS_HIGH()  gpio_set(ILI9488_CS_PORT, ILI9488_CS_PIN)
#define DC_DATA()  gpio_set(ILI9488_DC_PORT, ILI9488_DC_PIN)
#define DC_CMD()   gpio_clear(ILI9488_DC_PORT, ILI9488_DC_PIN)

/**
 * @brief Configures SPI1 and the control GPIOs, resets the panel and
 *        runs the ILI9488/ILI9486 init sequence (landscape MADCTL,
 *        RGB565, sleep out, display on). Call once.
 */
void ili9488_init(void);

/**
 * @brief Fills an axis-aligned rectangle on the panel with one RGB565 color.
 * @param x, y  Top-left corner.
 * @param w, h  Width and height in pixels.
 * @param color RGB565 color.
 */
void ili9488_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);

/**
 * @brief Sets one pixel on the panel directly (no framebuffer involved).
 * @param x, y  Pixel coordinates.
 * @param color RGB565 color.
 */
void ili9488_draw_pixel(uint16_t x, uint16_t y, uint16_t color);

/**
 * @brief Sets the panel's addressing window (column/row range) that
 *        subsequent pixel data writes will fill.
 * @param x0, y0 Top-left corner (inclusive).
 * @param x1, y1 Bottom-right corner (inclusive).
 */
void ili9488_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);

/**
 * @brief Sends a raw command byte (DC low) to the panel.
 * @param cmd Command byte.
 */
void ili9488_cmd(uint8_t cmd);

/**
 * @brief Sends a raw data byte (DC high) to the panel.
 * @param data Data byte.
 */
void ili9488_data(uint8_t data);

#endif
