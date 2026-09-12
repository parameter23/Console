/**
 * @file touch.c
 * @brief Implementation of the touch controller CS setup (see touch.h).
 */
#include "touch.h"

#include <libopencm3/stm32/rcc.h>

/** @brief See touch_init() in the header for the full contract. */
void touch_init(void)
{
    rcc_periph_clock_enable(RCC_GPIOB);

    gpio_mode_setup(TOUCH_CS_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, TOUCH_CS_PIN);
    gpio_set_output_options(TOUCH_CS_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, TOUCH_CS_PIN);
    gpio_set(TOUCH_CS_PORT, TOUCH_CS_PIN); /* deselected (active low) */
}
