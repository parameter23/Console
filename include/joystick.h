#ifndef JOYSTICK_H
#define JOYSTICK_H

#include <stdint.h>

// Digitale Richtungen (GPIOB)
#define JS_PORT        GPIOB
#define JS_UP_PIN      GPIO13
#define JS_DOWN_PIN    GPIO15
#define JS_LEFT_PIN    GPIO14
#define JS_RIGHT_PIN   GPIO10
#define JS_BTN_PIN     GPIO12

// Bitmasken für joystick_update().raw
#define JS_UP     0x01
#define JS_DOWN   0x02
#define JS_LEFT   0x04
#define JS_RIGHT  0x08
#define JS_BTN    0x10

// Analoge Pins (ADC)
#define JS_ADC_X_PORT  GPIOA
#define JS_ADC_X_PIN   GPIO0   // PA0 = ADC1_IN0

#define JS_ADC_Y_PORT  GPIOA
#define JS_ADC_Y_PIN   GPIO1   // PA1 = ADC1_IN1

typedef struct {
    uint8_t raw;       // aktueller Zustand
    uint8_t pressed;   // Flanke: gedrückt
    uint8_t released;  // Flanke: losgelassen
    uint8_t repeat;    // Auto-Repeat
} JoystickState;

void joystick_init(void);
JoystickState joystick_update(void);

// ADC Getter
uint16_t joystick_get_adc_x(void);
uint16_t joystick_get_adc_y(void);

#endif
