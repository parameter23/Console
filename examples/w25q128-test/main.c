/**
 * @file main.c
 * @brief Standalone hardware test for the onboard W25Q128 SPI flash (see
 *        w25q128.h). Reads the chip's JEDEC ID and checks it against the
 *        known-good W25Q128 signature (0xEF 0x40 0x18); the result is
 *        reported on PC13 (the BlackPill's onboard LED, active-low) since
 *        this board has no display attached during a bring-up test:
 *          - ID matches:    LED on solid.
 *          - ID mismatches: LED blinks fast, forever.
 */
#include "clock.h"
#include "spi.h"
#include "w25q128.h"

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/spi.h>

#define RESULT_PORT  GPIOC
#define RESULT_PIN   GPIO13

static void result_led_init(void)
{
    rcc_periph_clock_enable(RCC_GPIOC);
    gpio_mode_setup(RESULT_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, RESULT_PIN);
    gpio_set_output_options(RESULT_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_2MHZ, RESULT_PIN);
    gpio_set(RESULT_PORT, RESULT_PIN); /* active-low: start off */
}

static void fail_forever(void)
{
    for (;;) {
        gpio_toggle(RESULT_PORT, RESULT_PIN);
        delay_ms(100);
    }
}

int main(void)
{
    clock_setup();
    systick_setup();
    result_led_init();

    spi1_setup(SPI_CR1_BAUDRATE_FPCLK_DIV_4);
    w25q_init();

    uint8_t id[3];
    w25q_read_id(id);

    if (id[0] == 0xEF && id[1] == 0x40 && id[2] == 0x18) {
        gpio_clear(RESULT_PORT, RESULT_PIN); /* success: LED on */
    } else {
        fail_forever(); /* mismatch/no chip: fast blink */
    }

    for (;;) {
        /* Success - hold here with the LED lit. */
    }
}
