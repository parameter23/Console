/**
 * @file main.c
 * @brief Standalone hardware test for a W25Q80 SPI flash wired to the same
 *        CS/SCK/MISO/MOSI pins as the W25Q128 (see w25q128.h) - the W25Q80
 *        is command-set compatible with the rest of the Winbond W25Q
 *        family (same JEDEC-ID/read/program/erase opcodes, 256B pages,
 *        4KB sectors), just smaller, so the existing driver is reused
 *        as-is. Reads the chip's JEDEC ID and checks it against the
 *        known-good W25Q80 signature (0xEF 0x40 0x14); the result is
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

/** @brief JEDEC ID reported by a genuine W25Q80 (manufacturer 0xEF,
 *         memory type 0x40, capacity 0x14 = 8Mbit). */
static const uint8_t W25Q80_ID[3] = {0xEF, 0x40, 0x14};

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

    if (id[0] == W25Q80_ID[0] && id[1] == W25Q80_ID[1] && id[2] == W25Q80_ID[2]) {
        gpio_clear(RESULT_PORT, RESULT_PIN); /* success: LED on */
    } else {
        fail_forever(); /* mismatch/no chip: fast blink */
    }

    for (;;) {
        /* Success - hold here with the LED lit. */
    }
}
