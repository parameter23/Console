/**
 * @file sfx.h
 * @brief Small 3-voice pseudo-SID synth driving a PWM speaker output
 *        (TIM3 PWM carrier, TIM2-timed 20 kHz sample clock). Provides
 *        both canned sound effects and a raw-frequency voice used by
 *        music.c.
 */
#pragma once
#include <stdint.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/timer.h>

/* Hardware mapping */
#define SFX_TIMER       TIM3
#define SFX_TIMER_RCC   RCC_TIM3
#define SFX_GPIO_PORT   GPIOA
#define SFX_GPIO_PIN    GPIO6
#define SFX_GPIO_AF     GPIO_AF2

/**
 * @brief Canned sound effects playable via sfx_play().
 */
typedef enum {
    SFX_NONE = 0,
    SFX_BEEP,
    SFX_LASER,
    SFX_EXPLOSION,
    SFX_NOISE_SHORT,
    SFX_PICKUP,
    SFX_MOVE,
} SfxType;

/**
 * @brief Configures the PWM output pin/timer and the internal 20 kHz
 *        sample-rate timer/ISR. Call once.
 */
void sfx_init(void);

/**
 * @brief Advances the effect/music sequencer by one millisecond. Normally
 *        not called directly - the internal TIM2 ISR drives this at
 *        20 kHz via a divider, so SysTick is not needed for audio timing.
 */
void sfx_update_1ms(void);

/**
 * @brief Starts playing a canned sound effect on voice 1, replacing
 *        whatever was previously playing there.
 * @param type Effect to play.
 */
void sfx_play(SfxType type);

/** @brief Silences all three voices immediately. */
void sfx_stop(void);

/**
 * @brief Directly drives voice 0 (the "music" voice) at a fixed frequency;
 *        used by music.c to play MusicNote sequences. freq == 0 silences it.
 * @param freq Frequency in Hz, or 0 to silence.
 */
void sfx_play_freq(uint16_t freq);
