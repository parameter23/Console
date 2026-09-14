/**
 * @file sfx.c
 * @brief Implementation of the pseudo-SID synth (see sfx.h).
 */
#include "sfx.h"
#include "music.h"

#include <libopencm3/cm3/nvic.h>

/* --------- Pseudo-SID-Engine: 3 Stimmen --------- */

typedef enum {
    SID_WAVE_SQUARE,
    SID_WAVE_TRIANGLE,
    SID_WAVE_NOISE
} SidWave;

typedef struct {
    uint32_t freq;
    uint8_t  volume;     /* target/peak level the envelope ramps toward */
    uint8_t  env;        /* current envelope level actually heard - see
                           * envelope_tick(); this, not volume, drives
                           * the mixer, so notes/effects fade in and out
                           * instead of clicking on/off. */
    uint8_t  releasing;  /* 1 = ramping env down to 0, then auto-mutes */
    uint16_t pw;          /* SID_WAVE_SQUARE duty threshold, 0..0xFFFF;
                           * 0x8000 = 50%. Unused by other waveforms. */
    SidWave  wave;
    uint32_t phase;
    uint8_t  active;

    /* AY-3-8910-style extensions (see sfx_set_mixer()/
     * sfx_voice_use_envelope() in sfx.h) - only consulted for
     * SID_WAVE_SQUARE voices, and default to plain-tone/off so every
     * existing voice/effect behaves exactly as before unless a caller
     * opts in. */
    uint8_t  tone_enable;
    uint8_t  noise_enable;
    uint8_t  use_ay_env;
} SidVoice;

#define SID_VOICES       3
#define SID_SAMPLE_RATE  20000
#define SID_PHASE_BITS   24
#define SID_PHASE_MAX    (1u << SID_PHASE_BITS)
#define SID_PW_DEFAULT   0x8000

/* Envelope ramp rates, applied once per 1 ms tick (see envelope_tick()).
 * Attack is fast (a handful of ms to peak) just to avoid a hard click;
 * release is slower so effects/notes trail off instead of cutting out. */
#define SID_ENV_ATTACK_STEP   32
#define SID_ENV_RELEASE_STEP  6

/*
 * step = freq * SID_PHASE_MAX / SID_SAMPLE_RATE, precomputed as a Q16
 * fixed-point multiplier so sid_update() never has to do a runtime 64-bit
 * division. Cortex-M4 has no hardware 64-bit divider, and that division
 * would otherwise need libgcc's __aeabi_uldivmod - unavailable under this
 * project's -nostdlib link (no -lgcc in LDLIBS).
 *
 * STEP_PER_HZ_Q16 = (SID_PHASE_MAX << 16) / SID_SAMPLE_RATE, computed once
 * here at compile time by the preprocessor/compiler, not at runtime.
 */
#define SID_STEP_PER_HZ_Q16  ((uint32_t)(((uint64_t)SID_PHASE_MAX << 16) / SID_SAMPLE_RATE))

static SidVoice sid_voice[SID_VOICES];
static uint32_t sid_noise_lfsr = 0xACE1;

/* --------- AY-3-8910-style extensions --------- */

/* Shared noise generator: like sid_noise_lfsr above but clocked by its
 * own configurable-period phase accumulator (sfx_set_noise_period())
 * instead of advancing every sample - the real chip's noise generator
 * is period-controlled too, not free-running at the sample rate. Kept
 * entirely separate from sid_noise_lfsr so the legacy per-voice
 * SID_WAVE_NOISE effects (SFX_EXPLOSION/SFX_NOISE_SHORT) are unaffected. */
static uint32_t ay_noise_lfsr  = 0xACE1u;
static uint8_t  ay_noise_bit   = 0;
static uint32_t ay_noise_phase = 0;
static uint32_t ay_noise_step  = 0;

/* Shared envelope generator: one instance, like the real chip. Any
 * voice can opt in via sfx_voice_use_envelope(); voices that don't are
 * unaffected and keep using envelope_tick()'s own attack/release. */
static SfxEnvShape ay_env_shape   = SFX_ENV_NONE;
static uint8_t     ay_env_level   = 0;   /* 0..15 */
static uint16_t    ay_env_rate_ms = 20;
static uint16_t    ay_env_counter = 0;
static int8_t       ay_env_dir     = 1;

/* Effektzustand */
static SfxType  sfx_current      = SFX_NONE;
static uint32_t sfx_time_left_ms = 0;
static uint32_t sfx_param        = 0;

/* Teiler: 20 kHz → 1 kHz für SFX/Musik */
static uint16_t sfx_divider = 0;

/* ---------------------------------------------------------
 * Hardware-Init: TIM3 PWM + TIM2 Audio-Timer
 * --------------------------------------------------------- */
/** @brief See sfx_init() in the header for the full contract. */
void sfx_init(void)
{
    /* GPIOB PB4 = TIM3_CH1 (AF2) - see SFX_GPIO_PORT in sfx.h for why
     * this isn't PA6. Slew rate is still kept low deliberately: this
     * 40kHz PWM has no audible need for fast edges, and slow edges mean
     * less high-frequency harmonic content radiated/coupled to
     * neighbouring signals in general. */
    rcc_periph_clock_enable(RCC_GPIOB);
    gpio_mode_setup(SFX_GPIO_PORT, GPIO_MODE_AF, GPIO_PUPD_NONE, SFX_GPIO_PIN);
    gpio_set_af(SFX_GPIO_PORT, SFX_GPIO_AF, SFX_GPIO_PIN);
    gpio_set_output_options(SFX_GPIO_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_2MHZ, SFX_GPIO_PIN);

    /* TIM3 PWM (8-Bit) */
    rcc_periph_clock_enable(SFX_TIMER_RCC);
    timer_disable_counter(SFX_TIMER);

    uint32_t clk   = rcc_apb1_frequency * 2;   /* 100 MHz */
    uint32_t presc = clk / 1000000UL;          /* 1 MHz Tick */
    if (presc == 0) presc = 1;

    timer_set_prescaler(SFX_TIMER, presc - 1);
    timer_set_period(SFX_TIMER, 24);           /* 40kHz PWM */

    timer_set_oc_mode(SFX_TIMER, TIM_OC1, TIM_OCM_PWM1);
    timer_set_oc_polarity_high(SFX_TIMER, TIM_OC1);
    timer_enable_oc_output(SFX_TIMER, TIM_OC1);
    timer_enable_break_main_output(SFX_TIMER);

    timer_set_oc_value(SFX_TIMER, TIM_OC1, 128);   /* Mittelwert */

    timer_enable_counter(SFX_TIMER);

    /* TIM2 = Audio-Sample-Timer @ 20 kHz */
    rcc_periph_clock_enable(RCC_TIM2);
    timer_disable_counter(TIM2);

    uint32_t tim2_clk = rcc_apb1_frequency * 2;    /* 100 MHz */
    uint32_t period   = tim2_clk / SID_SAMPLE_RATE;

    timer_set_prescaler(TIM2, 0);
    timer_set_period(TIM2, period);

    timer_enable_irq(TIM2, TIM_DIER_UIE);
    nvic_enable_irq(NVIC_TIM2_IRQ);

    timer_enable_counter(TIM2);

    /* Stimmen zurücksetzen */
    for (int i = 0; i < SID_VOICES; i++) {
        sid_voice[i].active    = 0;
        sid_voice[i].phase     = 0;
        sid_voice[i].freq      = 0;
        sid_voice[i].volume    = 0;
        sid_voice[i].env       = 0;
        sid_voice[i].releasing = 0;
        sid_voice[i].pw        = SID_PW_DEFAULT;
        sid_voice[i].wave      = SID_WAVE_SQUARE;
        sid_voice[i].tone_enable  = 1;
        sid_voice[i].noise_enable = 0;
        sid_voice[i].use_ay_env   = 0;
    }

    sfx_current      = SFX_NONE;
    sfx_time_left_ms = 0;
    sfx_param        = 0;
    sfx_divider      = 0;

    ay_noise_lfsr  = 0xACE1u;
    ay_noise_bit   = 0;
    ay_noise_phase = 0;
    sfx_set_noise_period(4000);

    ay_env_shape   = SFX_ENV_NONE;
    ay_env_level   = 0;
    ay_env_rate_ms = 20;
    ay_env_counter = 0;
    ay_env_dir     = 1;
}

/**
 * @brief Advances one voice's envelope by one step (called once per 1 ms
 *        tick for every voice). Ramps env up toward volume while active
 *        and not releasing (attack), or down to 0 while releasing
 *        (release) - whichever avoids the hard on/off click a direct
 *        volume cut would cause. Auto-deactivates the voice once a
 *        release reaches 0.
 */
static void envelope_tick(SidVoice *v)
{
    if (v->use_ay_env)
        return; /* volume driven by ay_envelope_tick() instead */

    if (!v->active) {
        v->env = 0;
        return;
    }

    if (v->releasing) {
        if (v->env > SID_ENV_RELEASE_STEP) {
            v->env -= SID_ENV_RELEASE_STEP;
        } else {
            v->env = 0;
            v->active = 0;
        }
    } else if (v->env < v->volume) {
        uint16_t next = (uint16_t)v->env + SID_ENV_ATTACK_STEP;
        v->env = (next > v->volume) ? v->volume : (uint8_t)next;
    }
}

/**
 * @brief Advances the shared AY-3-8910-style envelope generator by one
 *        step every ay_env_rate_ms milliseconds, then pushes the
 *        resulting level (scaled 0..15 -> 0..255) into every voice
 *        that opted in via sfx_voice_use_envelope(). A no-op while the
 *        generator is off (SFX_ENV_NONE, the default).
 */
static void ay_envelope_tick(void)
{
    if (ay_env_shape == SFX_ENV_NONE)
        return;

    ay_env_counter++;
    if (ay_env_counter < ay_env_rate_ms)
        return;
    ay_env_counter = 0;

    switch (ay_env_shape) {
    case SFX_ENV_FADE_OUT:
        if (ay_env_level > 0) ay_env_level--;
        break;

    case SFX_ENV_FADE_IN:
        if (ay_env_level < 15) ay_env_level++;
        break;

    case SFX_ENV_SAWTOOTH:
        ay_env_level = (ay_env_level >= 15) ? 0 : (uint8_t)(ay_env_level + 1);
        break;

    case SFX_ENV_TRIANGLE:
        if (ay_env_dir > 0) {
            if (ay_env_level >= 15) { ay_env_level = 15; ay_env_dir = -1; }
            else ay_env_level++;
        } else {
            if (ay_env_level == 0) ay_env_dir = 1;
            else ay_env_level--;
        }
        break;

    default:
        break;
    }

    uint8_t scaled = (uint8_t)(ay_env_level * 17u); /* 0..15 -> 0..255 */
    for (int i = 0; i < SID_VOICES; i++)
        if (sid_voice[i].use_ay_env)
            sid_voice[i].env = scaled;
}

/* ---------------------------------------------------------
 * interner 1-ms-Tick für Effekte + Musik
 * (wird aus sid_update über Teiler aufgerufen)
 * --------------------------------------------------------- */
/** @brief Internal 1 ms tick for effects + music, called from
 *         sid_update() via a sample-rate divider. */
static void sfx_update_tick(void)
{
    /* Effekte */
    if (sfx_current != SFX_NONE) {
        if (sfx_time_left_ms > 0) {
            sfx_time_left_ms--;
            if (sfx_time_left_ms == 0) {
                /* Hand voice 1 back to its own attack/release fade and
                 * restore plain-tone mixer defaults, in case the effect
                 * that just ended (e.g. SFX_SIREN) opted into the
                 * AY-style mixer/shared envelope - otherwise a later,
                 * unrelated effect on this voice would inherit them. */
                sid_voice[1].use_ay_env   = 0;
                sid_voice[1].tone_enable  = 1;
                sid_voice[1].noise_enable = 0;
                sid_voice[1].releasing    = 1;
                sfx_current = SFX_NONE;
            }
        }

        SidVoice *v = &sid_voice[1];

        switch (sfx_current) {
        case SFX_PICKUP:
            /* Two-step "ding-ding" arpeggio (C5 -> G5) instead of one
             * continuous sweep - reads more like a satisfying pickup
             * chime than a siren. */
            sfx_param++;
            v->freq = (sfx_param < 35) ? 523 : 784;
            break;

        case SFX_MOVE:
            v->freq += (sfx_param & 1) ? 10 : -10;
            sfx_param++;
            break;

        case SFX_LASER:
            sfx_param += 20;
            if (sfx_param > 3000) sfx_param = 3000;
            v->freq = sfx_param;
            /* Pulse width widens as the pitch climbs - the square wave
             * goes from thin/buzzy to fuller, a cheap "powering up"
             * texture change alongside the pitch sweep. */
            v->pw = (uint16_t)(0x3000 + (sfx_param * 0x5000) / 3000);
            break;

        case SFX_EXPLOSION:
            sfx_param = (sfx_param * 1103515245u + 12345u);
            v->freq  = 100 + ((sfx_param >> 24) * 20);
            break;

        case SFX_NOISE_SHORT:
            sfx_param = (sfx_param * 1664525u + 1013904223u);
            v->freq  = 200 + (((sfx_param >> 16) & 0xFF) * 15);
            break;

        case SFX_SIREN: {
            /* Triangle-sweep the tone between two pitches for the
             * classic wailing-siren shape, layered on top of the
             * AY-style tone+noise mixer and the shared envelope's
             * tremolo (both set up once in sfx_play()'s SFX_SIREN
             * case, not touched again here). */
            sfx_param++;
            uint32_t t = sfx_param % 600;
            v->freq = (uint16_t)((t < 300) ? (300 + t) : (950 - t));
            break;
        }

        default:
            break;
        }
    }

    for (int i = 0; i < SID_VOICES; i++)
        envelope_tick(&sid_voice[i]);
    ay_envelope_tick();

    /* Musik-Timing */
    music_update_1ms();
}

/** @brief See sfx_update_1ms() in the header for the full contract. */
void sfx_update_1ms(void)
{
    sfx_update_tick();
}

/* ---------------------------------------------------------
 * SID-Mixer (20 kHz) – ruft alle 20 Ticks den 1-ms-Tick auf
 * --------------------------------------------------------- */
/** @brief SID-style 20 kHz voice mixer; calls sfx_update_tick() every
 *         20 samples (= 1 kHz) and writes the mixed sample to the PWM
 *         output compare register. */
static void sid_update(void)
{
    /* 20 kHz / 20 = 1 kHz → SFX/Musik-Tick */
    sfx_divider++;
    if (sfx_divider >= 20) {
        sfx_divider = 0;
        sfx_update_tick();
    }

    /* Shared noise generator: its own period-controlled phase
     * accumulator (see sfx_set_noise_period()), advanced unconditionally
     * every sample regardless of which voices (if any) mix it in, so its
     * timing doesn't depend on who's listening. */
    ay_noise_phase += ay_noise_step;
    if (ay_noise_phase >= SID_PHASE_MAX) {
        ay_noise_phase -= SID_PHASE_MAX;
        ay_noise_lfsr = (ay_noise_lfsr >> 1) ^ (-(ay_noise_lfsr & 1u) & 0xB400u);
        ay_noise_bit  = (uint8_t)(ay_noise_lfsr & 1u);
    }

    int32_t mix = 0;
    int any_active = 0;

    for (int i = 0; i < SID_VOICES; i++) {
        SidVoice *v = &sid_voice[i];
        if (!v->active || v->env == 0)
            continue;

        uint8_t sample = 128;
        int contributed = 0;

        switch (v->wave) {
        case SID_WAVE_SQUARE: {
            /* AY-3-8910-style mixer: tone_enable/noise_enable each
             * default to plain tone-only (see sfx_set_mixer()), so this
             * reduces to the exact old square-wave behavior unless a
             * caller opts into noise. When both are enabled, the real
             * chip's tone and noise bits are gated together (AND) - the
             * channel is only "on" where both are - rather than simply
             * added. */
            uint8_t tone_bit = 1, noise_bit = 1;

            if (v->tone_enable && v->freq != 0) {
                /* step = freq * SID_PHASE_MAX / SID_SAMPLE_RATE, done as
                 * a 32x32 multiply + shift (native UMULL) instead of a
                 * runtime 64-bit division - see SID_STEP_PER_HZ_Q16. */
                uint32_t step = (uint32_t)(((uint64_t)v->freq * SID_STEP_PER_HZ_Q16) >> 16);
                v->phase += step;
                /* Variable duty cycle (v->pw) instead of a fixed 50%
                 * split - lets effects/voices sweep their timbre, not
                 * just pitch. */
                uint16_t ph16 = (uint16_t)(v->phase >> 8);
                tone_bit = (ph16 < v->pw) ? 0 : 1;
                contributed = 1;
            }
            if (v->noise_enable) {
                noise_bit = ay_noise_bit;
                contributed = 1;
            }
            if (contributed)
                sample = (tone_bit & noise_bit) ? 255 : 0;
            break;
        }
        case SID_WAVE_TRIANGLE:
            if (v->freq != 0) {
                uint32_t step = (uint32_t)(((uint64_t)v->freq * SID_STEP_PER_HZ_Q16) >> 16);
                v->phase += step;
                sample = (v->phase >> 16) ^ ((v->phase >> 23) ? 0xFF : 0x00);
                contributed = 1;
            }
            break;
        case SID_WAVE_NOISE:
            if (v->freq != 0) {
                sid_noise_lfsr = (sid_noise_lfsr >> 1) ^ (-(sid_noise_lfsr & 1u) & 0xB400u);
                sample = sid_noise_lfsr & 0xFF;
                contributed = 1;
            }
            break;
        }

        if (!contributed)
            continue;

        any_active = 1;
        /* v->env (not v->volume) drives the mix - see envelope_tick()/
         * ay_envelope_tick(). */
        mix += (int32_t)(sample - 128) * v->env;
    }

    if (!any_active) {
        /* wirklich Stille → keine PWM-Umschaltung */
        timer_set_oc_value(SFX_TIMER, TIM_OC1, 0);
        return;
    }

    if (mix > 32767)  mix = 32767;
    if (mix < -32768) mix = -32768;

//    /* für deine Hardware: Offset um 0 herum, nicht um 128 „neutral“ */
//    int32_t out32 = (mix >> 8);      // -128 .. +127
//    out32 += 128;                    // 0 .. 255
//    if (out32 < 0)   out32 = 0;
//    if (out32 > 255) out32 = 255;

    int32_t out32 = (mix >> 8);   // -128 .. +127
    out32 = (out32 * 12) / 128 + 12;   // auf 0..24 skalieren, Mitte = 12
    if (out32 < 0) out32 = 0;
    if (out32 > 24) out32 = 24;

    uint16_t out = (uint16_t)out32;
    timer_set_oc_value(SFX_TIMER, TIM_OC1, out);
}

/** @brief TIM2 interrupt handler: fires at SID_SAMPLE_RATE (20 kHz) and
 *         drives the mixer. Not called directly. */
void tim2_isr(void)
{
    if (timer_get_flag(TIM2, TIM_SR_UIF)) {
        timer_clear_flag(TIM2, TIM_SR_UIF);
        sid_update();
    }
}

/* ---------------------------------------------------------
 * Musik (Voice 0 = Melodie, Voice 2 = optionaler Bass)
 * --------------------------------------------------------- */
/** @brief See sfx_play_freq() in the header for the full contract. */
void sfx_play_freq(uint16_t freq)
{
    if (freq == 0) {
        sid_voice[0].releasing = 1;
        return;
    }

    sid_voice[0].freq      = freq;
    sid_voice[0].volume    = 80;
    sid_voice[0].wave      = SID_WAVE_TRIANGLE;
    sid_voice[0].active    = 1;
    sid_voice[0].releasing = 0;
}

/** @brief See sfx_play_bass_freq() in the header for the full contract. */
void sfx_play_bass_freq(uint16_t freq)
{
    if (freq == 0) {
        sid_voice[2].releasing = 1;
        return;
    }

    sid_voice[2].freq      = freq;
    sid_voice[2].volume    = 65; /* a bit under the melody so it sits underneath */
    sid_voice[2].wave      = SID_WAVE_SQUARE;
    sid_voice[2].pw        = 0x9800; /* narrower duty - punchier low end */
    sid_voice[2].active    = 1;
    sid_voice[2].releasing = 0;
}

/* ---------------------------------------------------------
 * Effekte (Voice 1)
 * --------------------------------------------------------- */
/** @brief See sfx_play() in the header for the full contract. */
void sfx_play(SfxType type)
{
    sfx_current      = type;
    sfx_time_left_ms = 0;
    sfx_param        = 0;

    SidVoice *v = &sid_voice[1];
    v->releasing = 0;
    v->pw        = SID_PW_DEFAULT;

    switch (type) {
    case SFX_BEEP:
        v->wave   = SID_WAVE_SQUARE;
        v->volume = 100;
        v->freq   = 660;
        v->active = 1;
        sfx_time_left_ms = 90;
        break;

    case SFX_PICKUP:
        v->wave   = SID_WAVE_SQUARE;
        v->volume = 120;
        v->freq   = 523; /* C5, sfx_update_tick() arpeggios up to G5 */
        v->active = 1;
        sfx_param = 0;
        sfx_time_left_ms = 70;
        break;

    case SFX_MOVE:
        v->wave   = SID_WAVE_TRIANGLE;
        v->volume = 80;
        v->freq   = 180;
        v->active = 1;
        sfx_time_left_ms = 25;
        break;

    case SFX_LASER:
        v->wave   = SID_WAVE_SQUARE;
        v->volume = 120;
        v->freq   = 300;
        v->active = 1;
        sfx_param = 300;
        sfx_time_left_ms = 250;
        break;

    case SFX_EXPLOSION:
        v->wave   = SID_WAVE_NOISE;
        v->volume = 140;
        v->freq   = 400;
        v->active = 1;
        sfx_param = 0x12345678;
        sfx_time_left_ms = 400;
        break;

    case SFX_NOISE_SHORT:
        v->wave   = SID_WAVE_NOISE;
        v->volume = 120;
        v->freq   = 800;
        v->active = 1;
        sfx_param = 0x87654321;
        sfx_time_left_ms = 120;
        break;

    case SFX_SIREN:
        /* Classic AY-style wailing alarm: a pitch-swept square tone
         * (see sfx_update_tick()'s SFX_SIREN case) AND-gated with the
         * shared noise generator for a buzzy edge, with the shared
         * envelope generator driving a triangle tremolo on top instead
         * of this voice's usual single attack/release fade - all three
         * AY-3-8910-style extensions (mixer, noise period, envelope)
         * in one effect, in place of using them separately. */
        v->wave   = SID_WAVE_SQUARE;
        v->volume = 130;
        v->freq   = 300;
        v->active = 1;
        sfx_param = 0;
        sfx_time_left_ms = 1200;
        sfx_set_mixer(1, 1, 1);
        sfx_set_noise_period(300);
        sfx_set_envelope(SFX_ENV_TRIANGLE, 12);
        sfx_voice_use_envelope(1, 1);
        break;

    default:
        sfx_stop();
        break;
    }
}

/* ---------------------------------------------------------
 * Alles stumm
 * --------------------------------------------------------- */
/** @brief See sfx_stop() in the header for the full contract. */
void sfx_stop(void)
{
    for (int i = 0; i < SID_VOICES; i++) {
        sid_voice[i].active       = 0;
        sid_voice[i].freq         = 0;
        sid_voice[i].volume       = 0;
        sid_voice[i].env          = 0;
        sid_voice[i].releasing    = 0;
        sid_voice[i].tone_enable  = 1;
        sid_voice[i].noise_enable = 0;
        sid_voice[i].use_ay_env   = 0;
    }
    sfx_current      = SFX_NONE;
    sfx_time_left_ms = 0;
    sfx_param        = 0;

    ay_env_shape   = SFX_ENV_NONE;
    ay_env_level   = 0;
    ay_env_counter = 0;
    ay_env_dir     = 1;
}

/* ---------------------------------------------------------
 * AY-3-8910-style extensions (see sfx.h for the full contract of each)
 * --------------------------------------------------------- */

/** @brief See sfx_set_noise_period() in the header for the full contract. */
void sfx_set_noise_period(uint16_t period_hz)
{
    ay_noise_step = (uint32_t)(((uint64_t)period_hz * SID_STEP_PER_HZ_Q16) >> 16);
}

/** @brief See sfx_set_mixer() in the header for the full contract. */
void sfx_set_mixer(int voice, int tone_enable, int noise_enable)
{
    if (voice < 0 || voice >= SID_VOICES)
        return;

    sid_voice[voice].tone_enable  = (uint8_t)(tone_enable != 0);
    sid_voice[voice].noise_enable = (uint8_t)(noise_enable != 0);
}

/** @brief See sfx_set_envelope() in the header for the full contract. */
void sfx_set_envelope(SfxEnvShape shape, uint16_t rate_ms)
{
    ay_env_shape   = shape;
    ay_env_rate_ms = (rate_ms == 0) ? 1 : rate_ms;
    ay_env_counter = 0;
    ay_env_dir     = 1;
    ay_env_level   = (shape == SFX_ENV_FADE_OUT) ? 15 : 0;
}

/** @brief See sfx_voice_use_envelope() in the header for the full contract. */
void sfx_voice_use_envelope(int voice, int enable)
{
    if (voice < 0 || voice >= SID_VOICES)
        return;

    sid_voice[voice].use_ay_env = (uint8_t)(enable != 0);
}
