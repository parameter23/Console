/**
 * @file joystick.c
 * @brief Implementation of the digital/analog joystick driver (see joystick.h).
 */
#include "joystick.h"
#include "adc.h"

#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/rcc.h>

static uint16_t js_adc_x = 2048;
static uint16_t js_adc_y = 2048;

static uint32_t repeat_timer = 0;
static uint8_t  last_raw = 0;

/** @brief See joystick_init() in the header for the full contract. */
void joystick_init(void)
{
    rcc_periph_clock_enable(RCC_GPIOB);

    gpio_mode_setup(JS_PORT, GPIO_MODE_INPUT, GPIO_PUPD_PULLUP,
                     JS_UP_PIN | JS_DOWN_PIN | JS_LEFT_PIN |
                     JS_RIGHT_PIN | JS_BTN_PIN);

    hal_adc_pin_setup(JS_ADC_X_PORT, JS_ADC_X_PIN);
    hal_adc_pin_setup(JS_ADC_Y_PORT, JS_ADC_Y_PIN);
    hal_adc_init();
}

/** @brief See joystick_update() in the header for the full contract. */
JoystickState joystick_update(void)
{
    JoystickState js = {0};
    uint8_t raw = 0;

    if (!gpio_get(JS_PORT, JS_UP_PIN))    raw |= JS_UP;
    if (!gpio_get(JS_PORT, JS_DOWN_PIN))  raw |= JS_DOWN;
    if (!gpio_get(JS_PORT, JS_LEFT_PIN))  raw |= JS_LEFT;
    if (!gpio_get(JS_PORT, JS_RIGHT_PIN)) raw |= JS_RIGHT;
    if (!gpio_get(JS_PORT, JS_BTN_PIN))   raw |= JS_BTN;

    js.raw = raw;
    js.pressed  = (raw & ~last_raw);
    js.released = (~raw & last_raw);

    if (raw) {
        repeat_timer++;
        if (repeat_timer > 10) {
            js.repeat = raw;
            repeat_timer = 0;
        }
    } else {
        repeat_timer = 0;
    }

    last_raw = raw;

    /* PA0 = ADC1_IN0, PA1 = ADC1_IN1 */
    js_adc_x = hal_adc_read(0);
    js_adc_y = hal_adc_read(1);

    return js;
}

/** @brief See joystick_get_adc_x() in the header for the full contract. */
uint16_t joystick_get_adc_x(void) { return js_adc_x; }
/** @brief See joystick_get_adc_y() in the header for the full contract. */
uint16_t joystick_get_adc_y(void) { return js_adc_y; }
