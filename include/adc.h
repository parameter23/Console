/**
 * @file adc.h
 * @brief Small generic ADC1 wrapper (polled, single-conversion), shared
 *        by anything that needs an analog reading.
 */
#ifndef HAL_ADC_H
#define HAL_ADC_H

#include <stdint.h>

/**
 * @brief Configures ADC1 for single-conversion polled reads. Call once.
 */
void hal_adc_init(void);

/**
 * @brief Configures a GPIO pin as an analog input for ADC1. Call before
 *        or after hal_adc_init() - order doesn't matter as long as the
 *        GPIO port's clock is enabled by the time this runs (which it
 *        enables itself if needed).
 * @param gpioport The GPIOx port register base.
 * @param gpio_pin The GPIO pin(s) bitmask.
 */
void hal_adc_pin_setup(uint32_t gpioport, uint16_t gpio_pin);

/**
 * @brief Runs one blocking conversion on the given ADC1 channel.
 * @param channel ADC1 input channel (0..15).
 * @return The converted 12-bit sample (0..4095).
 */
uint16_t hal_adc_read(uint8_t channel);

#endif
