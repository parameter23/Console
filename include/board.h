#ifndef CLOCK_H
#define CLOCK_H

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

// SPI1 LCD
#define CS_LOW()   gpio_clear(GPIOA, GPIO4)
#define CS_HIGH()  gpio_set(GPIOA, GPIO4)
#define DC_DATA()  gpio_set(GPIOA, GPIO3)
#define DC_CMD()   gpio_clear(GPIOA, GPIO3)

// SPI1 SD-Card
#define SDCS_LOW()   gpio_clear(GPIOB, GPIO5)
#define SDCS_HIGH()  gpio_set(GPIOB, GPIO5)

#define SPI1_SCK PA5
#define SPI1_MISO PA6
#define SPI1_MOSI PA7

#endif
