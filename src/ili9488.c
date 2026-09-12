/**
 * @file ili9488.c
 * @brief Implementation of the ILI9488 panel driver (see ili9488.h).
 */
#include "ili9488.h"
#include "spi.h"
#include "clock.h"

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/spi.h>

/** @brief Configures the CS/DC/RST GPIOs used to talk to the panel. */
static void gpio_setup(void)
{
    rcc_periph_clock_enable(RCC_GPIOA);

    gpio_mode_setup(ILI9488_CS_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, ILI9488_CS_PIN);
    gpio_set_output_options(ILI9488_CS_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, ILI9488_CS_PIN);
    CS_HIGH();

    gpio_mode_setup(ILI9488_DC_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, ILI9488_DC_PIN | ILI9488_RST_PIN);
    gpio_set_output_options(ILI9488_DC_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_2MHZ, ILI9488_DC_PIN | ILI9488_RST_PIN);
}

/** @brief See ili9488_cmd() in the header for the full contract. */
void ili9488_cmd(uint8_t cmd)
{
    CS_LOW();
    DC_CMD();
    spi1_write8(cmd);
    CS_HIGH();
}

/** @brief See ili9488_data() in the header for the full contract. */
void ili9488_data(uint8_t data)
{
    CS_LOW();
    DC_DATA();
    spi1_write8(data);
    CS_HIGH();
}

/** @brief Toggles the RST pin to hardware-reset the panel. */
static void ili9488_reset(void)
{
    gpio_clear(ILI9488_RST_PORT, ILI9488_RST_PIN);
    delay_ms(20);
    gpio_set(ILI9488_RST_PORT, ILI9488_RST_PIN);
    delay_ms(20);
}

/** @brief See ili9488_init() in the header for the full contract. */
void ili9488_init(void)
{
    /* These panels are commonly specced for a lower max SPI clock than
     * ST7789 ones - DIV_4 (25 MHz on this project's 100 MHz APB2)
     * instead of the DIV_2 (50 MHz) ST7789 used. Raise it once this is
     * confirmed stable on real hardware. */
    spi1_setup(SPI_CR1_BAUDRATE_FPCLK_DIV_4);
    gpio_setup();
    ili9488_reset();

    /* Positive Gamma Control */
    ili9488_cmd(0xE0);
    static const uint8_t pgamma[] = {
        0x00, 0x03, 0x09, 0x08, 0x16, 0x0A, 0x3F, 0x78,
        0x4C, 0x09, 0x0A, 0x08, 0x16, 0x1A, 0x0F
    };
    for (unsigned i = 0; i < sizeof(pgamma); i++) ili9488_data(pgamma[i]);

    /* Negative Gamma Control */
    ili9488_cmd(0xE1);
    static const uint8_t ngamma[] = {
        0x00, 0x16, 0x19, 0x03, 0x0F, 0x05, 0x32, 0x45,
        0x46, 0x04, 0x0E, 0x0D, 0x35, 0x37, 0x0F
    };
    for (unsigned i = 0; i < sizeof(ngamma); i++) ili9488_data(ngamma[i]);

    ili9488_cmd(0xC0); /* Power Control 1 */
    ili9488_data(0x17); ili9488_data(0x15);

    ili9488_cmd(0xC1); /* Power Control 2 */
    ili9488_data(0x41);

    ili9488_cmd(0xC5); /* VCOM Control */
    ili9488_data(0x00); ili9488_data(0x12); ili9488_data(0x80);

    /* MADCTL: landscape (row/column exchange, BGR panel order).
     * If the image comes up mirrored or rotated on real hardware, try
     * 0x48, 0x88 or 0xE8 here instead - see ili9488.h. */
    ili9488_cmd(0x36); ili9488_data(0x28);

    ili9488_cmd(0x3A); ili9488_data(0x55); /* COLMOD: 16bpp RGB565 */

    ili9488_cmd(0xB0); /* Interface Mode Control */
    ili9488_data(0x00);

    ili9488_cmd(0xB1); /* Frame Rate Control */
    ili9488_data(0xA0);

    ili9488_cmd(0xB4); /* Display Inversion Control */
    ili9488_data(0x02);

    ili9488_cmd(0xB6); /* Display Function Control */
    ili9488_data(0x02); ili9488_data(0x02); ili9488_data(0x3B);

    ili9488_cmd(0xE9); /* Set Image Function */
    ili9488_data(0x00);

    ili9488_cmd(0xF7); /* Adjust Control 3 */
    ili9488_data(0xA9); ili9488_data(0x51); ili9488_data(0x2C); ili9488_data(0x82);

    ili9488_cmd(0x11);             /* Sleep Out */
    delay_ms(120);
    ili9488_cmd(0x29);             /* Display On */
}

/** @brief See ili9488_set_window() in the header for the full contract. */
void ili9488_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    ili9488_cmd(0x2A);
    ili9488_data(x0 >> 8); ili9488_data(x0 & 0xFF);
    ili9488_data(x1 >> 8); ili9488_data(x1 & 0xFF);

    ili9488_cmd(0x2B);
    ili9488_data(y0 >> 8); ili9488_data(y0 & 0xFF);
    ili9488_data(y1 >> 8); ili9488_data(y1 & 0xFF);

    ili9488_cmd(0x2C);
}

/** @brief See ili9488_draw_pixel() in the header for the full contract. */
void ili9488_draw_pixel(uint16_t x, uint16_t y, uint16_t color)
{
    ili9488_set_window(x, y, x, y);

    CS_LOW();
    DC_DATA();
    spi1_write8(color >> 8);
    spi1_write8(color & 0xFF);
    CS_HIGH();
}

/** @brief See ili9488_fill_rect() in the header for the full contract. */
void ili9488_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
    ili9488_set_window(x, y, x + w - 1, y + h - 1);

    CS_LOW();
    DC_DATA();
    for (uint32_t i = 0; i < (uint32_t)w * h; i++) {
        spi1_write8(color >> 8);
        spi1_write8(color & 0xFF);
    }
    CS_HIGH();
}
