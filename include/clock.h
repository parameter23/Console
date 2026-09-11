#ifndef HAL_CLOCK_H
#define HAL_CLOCK_H

#include <stdint.h>

/* System clock (HSE->PLL, 100 MHz AHB) + SysTick (1ms tick) */
void clock_setup(void);
void systick_setup(void);

uint32_t millis(void);
void delay_ms(uint32_t ms);

#endif
