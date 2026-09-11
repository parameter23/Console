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
    uint8_t  volume;
    SidWave  wave;
    uint32_t phase;
    uint8_t  active;
} SidVoice;

#define SID_VOICES       3
#define SID_SAMPLE_RATE  20000
#define SID_PHASE_BITS   24
#define SID_PHASE_MAX    (1u << SID_PHASE_BITS)

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
    /* GPIOA PA6 = TIM3_CH1 (AF2) */
    rcc_periph_clock_enable(RCC_GPIOA);
    gpio_mode_setup(SFX_GPIO_PORT, GPIO_MODE_AF, GPIO_PUPD_NONE, SFX_GPIO_PIN);
    gpio_set_af(SFX_GPIO_PORT, SFX_GPIO_AF, SFX_GPIO_PIN);
    gpio_set_output_options(SFX_GPIO_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, SFX_GPIO_PIN);

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
        sid_voice[i].active = 0;
        sid_voice[i].phase  = 0;
        sid_voice[i].freq   = 0;
        sid_voice[i].volume = 0;
        sid_voice[i].wave   = SID_WAVE_SQUARE;
    }

    sfx_current      = SFX_NONE;
    sfx_time_left_ms = 0;
    sfx_param        = 0;
    sfx_divider      = 0;

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
                sid_voice[1].active = 0;
                sid_voice[1].freq   = 0;
                sid_voice[1].volume = 0;
                sfx_current = SFX_NONE;
            }
        }

        SidVoice *v = &sid_voice[1];

        switch (sfx_current) {
        case SFX_PICKUP:
            sfx_param += 40;
            if (sfx_param > 2000) sfx_param = 2000;
            v->freq = sfx_param;
            break;

        case SFX_MOVE:
            v->freq += (sfx_param & 1) ? 10 : -10;
            sfx_param++;
            break;

        case SFX_LASER:
            sfx_param += 20;
            if (sfx_param > 3000) sfx_param = 3000;
            v->freq = sfx_param;
            break;

        case SFX_EXPLOSION:
            sfx_param = (sfx_param * 1103515245u + 12345u);
            v->freq  = 100 + ((sfx_param >> 24) * 20);
            break;

        case SFX_NOISE_SHORT:
            sfx_param = (sfx_param * 1664525u + 1013904223u);
            v->freq  = 200 + (((sfx_param >> 16) & 0xFF) * 15);
            break;

        default:
            break;
        }
    }

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

    int32_t mix = 0;
    int any_active = 0;

    for (int i = 0; i < SID_VOICES; i++) {
        SidVoice *v = &sid_voice[i];
        if (!v->active || v->volume == 0 || v->freq == 0) {
            v->active = 0;
            continue;
        }

        any_active = 1;

        /* step = freq * SID_PHASE_MAX / SID_SAMPLE_RATE, done as a 32x32
         * multiply + shift (native UMULL) instead of a runtime 64-bit
         * division - see SID_STEP_PER_HZ_Q16 above. */
        uint32_t step = (uint32_t)(((uint64_t)v->freq * SID_STEP_PER_HZ_Q16) >> 16);
        v->phase += step;

        uint8_t sample = 0;

        switch (v->wave) {
        case SID_WAVE_SQUARE:
            sample = (v->phase & (1u << 23)) ? 255 : 0;
            break;
        case SID_WAVE_TRIANGLE:
            sample = (v->phase >> 16) ^ ((v->phase >> 23) ? 0xFF : 0x00);
            break;
        case SID_WAVE_NOISE:
            sid_noise_lfsr = (sid_noise_lfsr >> 1) ^ (-(sid_noise_lfsr & 1u) & 0xB400u);
            sample = sid_noise_lfsr & 0xFF;
            break;
        }

        mix += (int32_t)(sample - 128) * v->volume;
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
 * Musik (Voice 0)
 * --------------------------------------------------------- */
/** @brief See sfx_play_freq() in the header for the full contract. */
void sfx_play_freq(uint16_t freq)
{
    if (freq == 0) {
        sid_voice[0].active = 0;
        sid_voice[0].freq   = 0;
        sid_voice[0].volume = 0;
        return;
    }

    sid_voice[0].freq   = freq;
    sid_voice[0].volume = 80;
    sid_voice[0].wave   = SID_WAVE_TRIANGLE;
    sid_voice[0].active = 1;
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

    switch (type) {
    case SFX_PICKUP:
        v->wave   = SID_WAVE_SQUARE;
        v->volume = 120;
        v->freq   = 400;
        v->active = 1;
        sfx_param = 400;
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
        sid_voice[i].active = 0;
        sid_voice[i].freq   = 0;
        sid_voice[i].volume = 0;
    }
    sfx_current      = SFX_NONE;
    sfx_time_left_ms = 0;
    sfx_param        = 0;
}
