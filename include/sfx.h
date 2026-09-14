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

/* Hardware mapping.
 *
 * PB4 (not PA6) on purpose: TIM3_CH1 is available on both pins (AF2),
 * but PA6 sits right between SPI1's SCK (PA5) and MOSI (PA7) - fast
 * PWM edges there coupled onto the display SPI link and corrupted the
 * framebuffer on real hardware. PB4 is a different GPIO port, away
 * from the SPI bus, with no such crosstalk path.
 *
 * Note: PB4 is NJTRST out of reset. Harmless as long as programming
 * stays on 2-wire SWD (this project's openocd flash target already
 * uses cmsis-dap/SWD, not full JTAG) - sfx_init() reconfigures the pin
 * to AF2 before it's ever used as PWM. */
#define SFX_TIMER       TIM3
#define SFX_TIMER_RCC   RCC_TIM3
#define SFX_GPIO_PORT   GPIOB
#define SFX_GPIO_PIN    GPIO4
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
    SFX_SIREN,   /**< classic AY-style wailing alarm - demonstrates the
                   *   tone+noise mixer and shared envelope generator
                   *   below; see sfx_play()'s SFX_SIREN case. */
} SfxType;

/**
 * @brief Shapes for the shared AY-3-8910-style envelope generator (see
 *        sfx_set_envelope()) - a rough equivalent of the real chip's
 *        envelope shapes, not a register-accurate reproduction.
 */
typedef enum {
    SFX_ENV_NONE = 0,   /**< generator off; voices use their own
                          *   attack/release fade instead (default). */
    SFX_ENV_FADE_OUT,   /**< one-shot ramp 15 -> 0, then stays silent. */
    SFX_ENV_FADE_IN,    /**< one-shot ramp 0 -> 15, then stays at peak. */
    SFX_ENV_SAWTOOTH,   /**< repeating ramp 0 -> 15, then jumps back to 0. */
    SFX_ENV_TRIANGLE,   /**< repeating ramp 0 -> 15 -> 0. */
} SfxEnvShape;

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
 *        used by music.c to play MusicNote sequences. freq == 0 releases
 *        it into a short fade-out instead of cutting it off immediately
 *        (avoids an audible click at rests/note boundaries).
 * @param freq Frequency in Hz, or 0 to release into silence.
 */
void sfx_play_freq(uint16_t freq);

/**
 * @brief Directly drives voice 2 (the optional "bass" voice) at a fixed
 *        frequency; used by music.c's optional second music channel
 *        (see music_init_bass()). Same fade-out-on-zero behavior as
 *        sfx_play_freq(). Idle/silent until first used, so games that
 *        don't call music_init_bass() are unaffected.
 * @param freq Frequency in Hz, or 0 to release into silence.
 */
void sfx_play_bass_freq(uint16_t freq);

/* ---------------------------------------------------------
 * AY-3-8910-style extensions: a shared, period-controlled noise
 * generator and per-voice tone/noise mixer bits, plus a shared
 * envelope generator any voice can opt into. These add the chip's
 * general *capabilities* (tone+noise mixing, envelope shapes) on top
 * of the existing 3-voice engine - not a register- or file-compatible
 * emulation of the real chip.
 * --------------------------------------------------------- */

/**
 * @brief Sets the shared noise generator's pitch (like the real chip's
 *        noise period register, expressed here directly in Hz). Only
 *        affects voices with noise mixed in via sfx_set_mixer();
 *        everything else is unaffected. Defaults to a fixed pitch at
 *        sfx_init(), so existing code that never calls this is
 *        unaffected either way.
 * @param period_hz Noise generator rate in Hz.
 */
void sfx_set_noise_period(uint16_t period_hz);

/**
 * @brief AY-3-8910-style per-voice mixer. Only meaningful for voices
 *        using the square waveform (SID_WAVE_SQUARE internally - that's
 *        every voice unless music.c's triangle-wave melody/bass is
 *        driving it): selects whether the voice outputs its own tone,
 *        the shared noise generator (see sfx_set_noise_period()), both
 *        combined (real chip: tone and noise bits gated together, so
 *        the channel is only "on" when both are), or neither. Off
 *        (tone only, no noise) by default for every voice, so existing
 *        code that never calls this is unaffected.
 * @param voice        0 (melody), 1 (effects) or 2 (bass).
 * @param tone_enable  1 to include the voice's own tone.
 * @param noise_enable 1 to mix in the shared noise generator.
 */
void sfx_set_mixer(int voice, int tone_enable, int noise_enable);

/**
 * @brief Configures the shared envelope generator (one instance, like
 *        the real chip) and (re)starts it running. Call
 *        sfx_voice_use_envelope() to have a voice's volume follow it
 *        instead of its own attack/release fade.
 * @param shape   Envelope shape, or SFX_ENV_NONE to stop it.
 * @param rate_ms Milliseconds per one of the 16 envelope steps (lower =
 *                faster, matching the real chip's frequency-like
 *                envelope rate register).
 */
void sfx_set_envelope(SfxEnvShape shape, uint16_t rate_ms);

/**
 * @brief Selects whether voice `voice` takes its volume from the
 *        shared envelope generator (see sfx_set_envelope()) instead of
 *        its own attack/release fade. Off by default for every voice,
 *        so existing code that never calls this is unaffected.
 * @param voice  0 (melody), 1 (effects) or 2 (bass).
 * @param enable 1 to follow the shared envelope, 0 for normal behavior.
 */
void sfx_voice_use_envelope(int voice, int enable);
