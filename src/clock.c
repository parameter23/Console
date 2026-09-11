/**
 * @file clock.c
 * @brief Implementation of the system clock and millisecond time base
 *        (see clock.h).
 */
#include "clock.h"

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/flash.h>
#include <libopencm3/cm3/systick.h>

/** @brief Millisecond counter incremented by sys_tick_handler(). */
static volatile uint32_t ms_ticks = 0;

/**
 * @brief SysTick interrupt handler (weak alias resolved by libopencm3's
 *        startup code via the NVIC vector table). Not called directly.
 */
void sys_tick_handler(void)
{
    ms_ticks++;
}

/** @brief See millis() in the header for the full contract. */
uint32_t millis(void)
{
    return ms_ticks;
}

/** @brief See delay_ms() in the header for the full contract. */
void delay_ms(uint32_t ms)
{
    uint32_t start = millis();
    while ((millis() - start) < ms) {
        /* busy wait */
    }
}

/** @brief See clock_setup() in the header for the full contract. */
void clock_setup(void)
{
    /* HSE (25 MHz) -> PLL -> 100 MHz SYSCLK */
    rcc_osc_on(RCC_HSE);
    rcc_wait_for_osc_ready(RCC_HSE);

    RCC_CFGR &= ~(RCC_CFGR_HPRE_MASK | RCC_CFGR_PPRE1_MASK | RCC_CFGR_PPRE2_MASK);
    RCC_CFGR |= RCC_CFGR_HPRE_NODIV;  /* AHB  = SYSCLK      */
    RCC_CFGR |= RCC_CFGR_PPRE_DIV2;   /* APB1 = AHB / 2     */
    RCC_CFGR |= RCC_CFGR_PPRE_NODIV;  /* APB2 = AHB         */

    RCC_PLLCFGR =
          RCC_PLLCFGR_PLLSRC                    /* HSE as PLL source   */
        | (25  << RCC_PLLCFGR_PLLM_SHIFT)
        | (200 << RCC_PLLCFGR_PLLN_SHIFT)
        | (0   << RCC_PLLCFGR_PLLP_SHIFT)        /* PLLP = 2            */
        | (4   << RCC_PLLCFGR_PLLQ_SHIFT);

    rcc_osc_on(RCC_PLL);
    rcc_wait_for_osc_ready(RCC_PLL);

    flash_set_ws(FLASH_ACR_LATENCY_3WS);

    rcc_set_sysclk_source(RCC_CFGR_SW_PLL);

    rcc_ahb_frequency  = 100000000;
    rcc_apb1_frequency = 50000000;
    rcc_apb2_frequency = 100000000;
}

/** @brief See systick_setup() in the header for the full contract. */
void systick_setup(void)
{
    /* 100000 ticks @ 100 MHz AHB = 1 ms */
    systick_set_reload(100000 - 1);
    systick_set_clocksource(STK_CSR_CLKSOURCE_AHB);
    systick_clear();
    systick_counter_enable();
    systick_interrupt_enable();
}
