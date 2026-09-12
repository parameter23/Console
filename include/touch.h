/**
 * @file touch.h
 * @brief Chip-select for the resistive touch controller (XPT2046/
 *        ADS7846-compatible) built into the 3.5" LCD module.
 *
 * The touch controller shares SPI1's SCK/MOSI (PA5/PA7) with the
 * display and the W25Q128 flash (see ili9488.h/w25q128.h) - its SO
 * line goes to the same MISO (PA6) the flash chip uses. As with those
 * two, sharing the clock/data lines is fine as long as each device has
 * its own CS, which is all this header adds so far: no transfer
 * function yet, and the module's TP_IRQ line isn't wired up either.
 */
#ifndef TOUCH_H
#define TOUCH_H

#include <stdint.h>
#include <libopencm3/stm32/gpio.h>

#define TOUCH_CS_PORT  GPIOB
#define TOUCH_CS_PIN   GPIO6

/**
 * @brief Configures the touch controller's CS pin as an output, held
 *        high (deselected). SPI1 itself must already be running (see
 *        ili9488_init()/spi1_setup()) before any future transfer code
 *        pulls this low.
 */
void touch_init(void);

#endif
