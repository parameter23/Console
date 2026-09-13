/**
 * @file main.c
 * @brief Pin-identification test for the flash's four SPI1 lines. Drives
 *        PA4/PA5/PA6/PA7 as plain GPIO outputs (bypassing the SPI
 *        peripheral entirely) and pulses each one a distinct number of
 *        times in turn, forever:
 *          PA4 (CS)   - 1 pulse
 *          PA5 (SCK)  - 2 pulses
 *          PA6 (MISO) - 3 pulses
 *          PA7 (MOSI) - 4 pulses
 *        Probe each of the flash chip's corresponding legs with a meter
 *        or scope and count the pulses to confirm that pin actually
 *        carries the signal from the matching MCU pin - catches a
 *        swapped/miswired trace that a plain continuity check on the
 *        net could miss (e.g. two shorted traces would still "beep"
 *        continuous on a continuity tester).
 */
#include "clock.h"

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>

#define PA4  GPIO4  /* CS */
#define PA5  GPIO5  /* SCK */
#define PA6  GPIO6  /* MISO */
#define PA7  GPIO7  /* MOSI */

static void pulse(uint16_t pin, unsigned count)
{
    for (unsigned i = 0; i < count; i++) {
        gpio_set(GPIOA, pin);
        delay_ms(150);
        gpio_clear(GPIOA, pin);
        delay_ms(150);
    }
}

int main(void)
{
    clock_setup();
    systick_setup();

    rcc_periph_clock_enable(RCC_GPIOA);
    gpio_mode_setup(GPIOA, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, PA4 | PA5 | PA6 | PA7);
    gpio_set_output_options(GPIOA, GPIO_OTYPE_PP, GPIO_OSPEED_2MHZ, PA4 | PA5 | PA6 | PA7);
    gpio_clear(GPIOA, PA4 | PA5 | PA6 | PA7);

    for (;;) {
        pulse(PA4, 1);
        delay_ms(600);
        pulse(PA5, 2);
        delay_ms(600);
        pulse(PA6, 3);
        delay_ms(600);
        pulse(PA7, 4);
        delay_ms(1200);
    }
}
