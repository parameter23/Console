/**
 * @file st7789.c
 * @brief Implementation of the ST7789 panel driver (see st7789.h).
 *
 * NOTE: this used to configure SPI1 itself (with a different baud-rate
 * divider than hal/spi.c). That duplicate/conflicting init is gone now -
 * st7789_init() just calls spi1_setup() once, like everything else that
 * needs SPI1 should.
 */
#include "st7789.h"
#include "spi.h"
#include "clock.h"

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/spi.h>

/** @brief Configures the CS/DC/RST GPIOs used to talk to the panel. */
static void gpio_setup(void)
{
    rcc_periph_clock_enable(RCC_GPIOA);

    gpio_mode_setup(ST7789_CS_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, ST7789_CS_PIN);
    gpio_set_output_options(ST7789_CS_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, ST7789_CS_PIN);
    CS_HIGH();

    gpio_mode_setup(ST7789_DC_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, ST7789_DC_PIN | ST7789_RST_PIN);
    gpio_set_output_options(ST7789_DC_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_2MHZ, ST7789_DC_PIN | ST7789_RST_PIN);
}

/** @brief See st7789_cmd() in the header for the full contract. */
void st7789_cmd(uint8_t cmd)
{
    CS_LOW();
    DC_CMD();
    spi1_write8(cmd);
    CS_HIGH();
}

/** @brief See st7789_data() in the header for the full contract. */
void st7789_data(uint8_t data)
{
    CS_LOW();
    DC_DATA();
    spi1_write8(data);
    CS_HIGH();
}

/** @brief Toggles the RST pin to hardware-reset the panel. */
static void st7789_reset(void)
{
    gpio_clear(ST7789_RST_PORT, ST7789_RST_PIN);
    delay_ms(20);
    gpio_set(ST7789_RST_PORT, ST7789_RST_PIN);
    delay_ms(20);
}

/** @brief See st7789_init() in the header for the full contract. */
void st7789_init(void)
{
    spi1_setup(SPI_CR1_BAUDRATE_FPCLK_DIV_2);
    gpio_setup();
    st7789_reset();

    st7789_cmd(0x36); st7789_data(0x70);   /* MADCTL: landscape          */
    st7789_cmd(0x3A); st7789_data(0x55);   /* COLMOD: 16bpp RGB565       */

    st7789_cmd(0xB2);
    st7789_data(0x0C); st7789_data(0x0C); st7789_data(0x00);
    st7789_data(0x33); st7789_data(0x33);

    st7789_cmd(0xB7); st7789_data(0x35);
    st7789_cmd(0xBB); st7789_data(0x2B);
    st7789_cmd(0xC0); st7789_data(0x2C);
    st7789_cmd(0xC2); st7789_data(0x01);
    st7789_cmd(0xC3); st7789_data(0x0B);
    st7789_cmd(0xC4); st7789_data(0x20);
    st7789_cmd(0xC6); st7789_data(0x0F);

    st7789_cmd(0xD0);
    st7789_data(0xA4); st7789_data(0xA1);

    st7789_cmd(0x21);              /* Display Inversion On */
    st7789_cmd(0x11);              /* Sleep Out             */
    delay_ms(120);
    st7789_cmd(0x29);              /* Display On             */
}

/** @brief See st7789_set_window() in the header for the full contract. */
void st7789_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    st7789_cmd(0x2A);
    st7789_data(x0 >> 8); st7789_data(x0 & 0xFF);
    st7789_data(x1 >> 8); st7789_data(x1 & 0xFF);

    st7789_cmd(0x2B);
    st7789_data(y0 >> 8); st7789_data(y0 & 0xFF);
    st7789_data(y1 >> 8); st7789_data(y1 & 0xFF);

    st7789_cmd(0x2C);
}

/** @brief See st7789_draw_pixel() in the header for the full contract. */
void st7789_draw_pixel(uint16_t x, uint16_t y, uint16_t color)
{
    st7789_set_window(x, y, x, y);

    CS_LOW();
    DC_DATA();
    spi1_write8(color >> 8);
    spi1_write8(color & 0xFF);
    CS_HIGH();
}

/** @brief See st7789_fill_rect() in the header for the full contract. */
void st7789_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
    st7789_set_window(x, y, x + w - 1, y + h - 1);

    CS_LOW();
    DC_DATA();
    for (uint32_t i = 0; i < (uint32_t)w * h; i++) {
        spi1_write8(color >> 8);
        spi1_write8(color & 0xFF);
    }
    CS_HIGH();
}
