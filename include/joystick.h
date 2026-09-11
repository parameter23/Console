/**
 * @file joystick.h
 * @brief Digital joystick (5 buttons on GPIOB) with debounced edges and
 *        auto-repeat, plus its two analog axes.
 */
#ifndef JOYSTICK_H
#define JOYSTICK_H

#include <stdint.h>

/* Digital directions (GPIOB) */
#define JS_PORT        GPIOB
#define JS_UP_PIN      GPIO13
#define JS_DOWN_PIN    GPIO15
#define JS_LEFT_PIN    GPIO14
#define JS_RIGHT_PIN   GPIO10
#define JS_BTN_PIN     GPIO12

/* Bitmasks for JoystickState.raw/pressed/released/repeat */
#define JS_UP     0x01
#define JS_DOWN   0x02
#define JS_LEFT   0x04
#define JS_RIGHT  0x08
#define JS_BTN    0x10

/* Analog pins (ADC) */
#define JS_ADC_X_PORT  GPIOA
#define JS_ADC_X_PIN   GPIO0   /* PA0 = ADC1_IN0 */

#define JS_ADC_Y_PORT  GPIOA
#define JS_ADC_Y_PIN   GPIO1   /* PA1 = ADC1_IN1 */

/**
 * @brief Snapshot of the digital joystick, as returned by joystick_update().
 */
typedef struct {
    uint8_t raw;       /**< Currently held buttons (JS_* bitmask). */
    uint8_t pressed;   /**< Buttons that went down this call (edge). */
    uint8_t released;  /**< Buttons that went up this call (edge). */
    uint8_t repeat;    /**< Buttons firing an auto-repeat pulse this call. */
} JoystickState;

/**
 * @brief Configures the digital button GPIOs and the two analog axis pins,
 *        and initialises the shared ADC. Call once.
 */
void joystick_init(void);

/**
 * @brief Polls the digital buttons and analog axes and computes edges/
 *        auto-repeat relative to the previous call. Call once per loop
 *        iteration.
 * @return The current joystick state.
 */
JoystickState joystick_update(void);

/**
 * @brief Last analog X reading taken by joystick_update().
 * @return Raw ADC1 value (0..4095).
 */
uint16_t joystick_get_adc_x(void);

/**
 * @brief Last analog Y reading taken by joystick_update().
 * @return Raw ADC1 value (0..4095).
 */
uint16_t joystick_get_adc_y(void);

#endif
