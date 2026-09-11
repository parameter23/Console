#include "adc.h"

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/adc.h>

void hal_adc_pin_setup(uint32_t gpioport, uint16_t gpio_pin)
{
    /* Caller is expected to have already enabled the GPIO port's clock,
     * but enabling it again here is harmless and makes the function
     * usable standalone. */
    if (gpioport == GPIOA) rcc_periph_clock_enable(RCC_GPIOA);
    if (gpioport == GPIOB) rcc_periph_clock_enable(RCC_GPIOB);
    if (gpioport == GPIOC) rcc_periph_clock_enable(RCC_GPIOC);

    gpio_mode_setup(gpioport, GPIO_MODE_ANALOG, GPIO_PUPD_NONE, gpio_pin);
}

void hal_adc_init(void)
{
    rcc_periph_clock_enable(RCC_ADC1);

    adc_power_off(ADC1);
    adc_disable_scan_mode(ADC1);
    adc_set_sample_time_on_all_channels(ADC1, ADC_SMPR_SMP_144CYC);
    adc_power_on(ADC1);
}

uint16_t hal_adc_read(uint8_t channel)
{
    uint8_t ch = channel;
    adc_set_regular_sequence(ADC1, 1, &ch);
    adc_start_conversion_regular(ADC1);
    while (!adc_eoc(ADC1))
        ;
    return adc_read_regular(ADC1);
}
