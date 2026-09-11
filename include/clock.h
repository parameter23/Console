/**
 * @file clock.h
 * @brief System clock (HSE -> PLL, 100 MHz AHB) and a 1 ms SysTick time base.
 */
#ifndef HAL_CLOCK_H
#define HAL_CLOCK_H

#include <stdint.h>

/**
 * @brief Configures HSE -> PLL to reach 100 MHz AHB (APB1 = 50 MHz,
 *        APB2 = 100 MHz). Call once, before any peripheral that depends
 *        on the resulting clock frequencies.
 */
void clock_setup(void);

/**
 * @brief Configures SysTick for a 1 ms interrupt period on top of the
 *        100 MHz AHB clock set up by clock_setup(). Call once, after
 *        clock_setup().
 */
void systick_setup(void);

/**
 * @brief Milliseconds elapsed since systick_setup(), wrapping every
 *        ~49.7 days like any uint32_t counter.
 * @return Current millisecond counter value.
 */
uint32_t millis(void);

/**
 * @brief Busy-waits for the given number of milliseconds.
 * @param ms Milliseconds to wait.
 */
void delay_ms(uint32_t ms);

#endif
