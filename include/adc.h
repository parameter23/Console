#ifndef HAL_ADC_H
#define HAL_ADC_H

#include <stdint.h>

/*
 * Small generic ADC1 wrapper, extracted out of joystick.c so it can be
 * reused by anything else that needs an analog reading later.
 */

/* Configure ADC1 for single-conversion polled reads. Call once. */
void hal_adc_init(void);

/* Configure a GPIO pin as analog input (call before hal_adc_init or after,
 * order doesn't matter as long as clocks are enabled). */
void hal_adc_pin_setup(uint32_t gpioport, uint16_t gpio_pin);

/* Blocking single conversion on the given ADC channel (0..15). */
uint16_t hal_adc_read(uint8_t channel);

#endif
