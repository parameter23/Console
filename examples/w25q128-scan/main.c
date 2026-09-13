/**
 * @file main.c
 * @brief Auto-detect which SPI bus/CS pin an onboard W25Q128 flash is
 *        actually wired to. Different "BlackPill" clones route the
 *        optional SPI-flash footprint differently - some use SPI1 with
 *        CS=PA4 (this project's default, see w25q128.h), others put the
 *        flash on SPI2 and use PB12 (SPI2's own NSS pin) or PB2 (BOOT1)
 *        as CS. This test tries each known combination in turn and
 *        checks for the exact W25Q128 JEDEC ID (0xEF 0x40 0x18) -
 *        unlike examples/w25q-scan, which only checks the generic
 *        Winbond manufacturer/type bytes and would also accept any
 *        other family member (e.g. a W25Q80).
 *
 * Talks to the SPI peripherals directly instead of going through
 * spi.c/w25q128.c, since those are hard-wired to SPI1/PA4.
 *
 * Result on PC13 (BlackPill onboard LED, active-low):
 *   - combo N works: blink N times, pause, repeat forever.
 *   - nothing found: fast continuous blink, forever (no pauses - easy
 *     to tell apart from the "N blinks + pause" success pattern).
 */
#include "clock.h"

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/spi.h>

#define LED_PORT  GPIOC
#define LED_PIN   GPIO13

#define CMD_READ_JEDEC_ID  0x9F

/** @brief JEDEC ID reported by a genuine W25Q128 (manufacturer 0xEF,
 *         memory type 0x40, capacity 0x18 = 128Mbit). */
static const uint8_t W25Q128_ID[3] = {0xEF, 0x40, 0x18};

typedef struct {
    uint32_t spi;       /* SPI1 or SPI2 peripheral base */
    uint32_t io_port;   /* Port carrying that SPI's SCK/MISO/MOSI */
    uint16_t io_pins;   /* SCK|MISO|MOSI pin mask on io_port, AF5 */
    uint32_t cs_port;
    uint16_t cs_pin;
} spi_combo_t;

static const spi_combo_t combos[] = {
    /* 1: SPI1 (PA5/6/7), CS=PA4 - this project's own default wiring. */
    { SPI1, GPIOA, GPIO5 | GPIO6 | GPIO7, GPIOA, GPIO4 },
    /* 2: SPI1 (PA5/6/7), CS=PB2 - some clones use BOOT1 as flash CS. */
    { SPI1, GPIOA, GPIO5 | GPIO6 | GPIO7, GPIOB, GPIO2 },
    /* 3: SPI2 (PB13/14/15), CS=PB12 - SPI2's own NSS pin used as CS. */
    { SPI2, GPIOB, GPIO13 | GPIO14 | GPIO15, GPIOB, GPIO12 },
    /* 4: SPI2 (PB13/14/15), CS=PB2 - flash on SPI2, CS on BOOT1. */
    { SPI2, GPIOB, GPIO13 | GPIO14 | GPIO15, GPIOB, GPIO2 },
};
#define NUM_COMBOS (sizeof(combos) / sizeof(combos[0]))

static void led_init(void)
{
    rcc_periph_clock_enable(RCC_GPIOC);
    gpio_mode_setup(LED_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, LED_PIN);
    gpio_set_output_options(LED_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_2MHZ, LED_PIN);
    gpio_set(LED_PORT, LED_PIN); /* active-low: start off */
}

/** @brief Configures one candidate bus/CS combo and checks for the
 *         exact W25Q128 JEDEC ID. */
static int test_combo(const spi_combo_t *c)
{
    gpio_mode_setup(c->io_port, GPIO_MODE_AF, GPIO_PUPD_NONE, c->io_pins);
    gpio_set_af(c->io_port, GPIO_AF5, c->io_pins);
    gpio_set_output_options(c->io_port, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, c->io_pins);

    gpio_mode_setup(c->cs_port, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, c->cs_pin);
    gpio_set_output_options(c->cs_port, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, c->cs_pin);
    gpio_set(c->cs_port, c->cs_pin); /* idle high */

    spi_disable(c->spi);
    spi_init_master(
        c->spi,
        SPI_CR1_BAUDRATE_FPCLK_DIV_32, /* slow/safe for unverified wiring */
        SPI_CR1_CPOL_CLK_TO_1_WHEN_IDLE,
        SPI_CR1_CPHA_CLK_TRANSITION_1,
        SPI_CR1_DFF_8BIT,
        SPI_CR1_MSBFIRST
    );
    spi_enable(c->spi);

    gpio_clear(c->cs_port, c->cs_pin);
    spi_send(c->spi, CMD_READ_JEDEC_ID);
    spi_read(c->spi);
    uint8_t id[3];
    for (int i = 0; i < 3; i++) {
        spi_send(c->spi, 0xFF);
        id[i] = spi_read(c->spi);
    }
    gpio_set(c->cs_port, c->cs_pin);

    spi_disable(c->spi);

    return (id[0] == W25Q128_ID[0] && id[1] == W25Q128_ID[1] && id[2] == W25Q128_ID[2]);
}

/** @brief Blinks `count` short pulses, then a long pause, forever. */
static void report_success(unsigned count)
{
    for (;;) {
        for (unsigned i = 0; i < count; i++) {
            gpio_clear(LED_PORT, LED_PIN);
            delay_ms(200);
            gpio_set(LED_PORT, LED_PIN);
            delay_ms(200);
        }
        delay_ms(1200);
    }
}

static void report_failure(void)
{
    for (;;) {
        gpio_toggle(LED_PORT, LED_PIN);
        delay_ms(100);
    }
}

int main(void)
{
    clock_setup();
    systick_setup();
    led_init();

    rcc_periph_clock_enable(RCC_GPIOA);
    rcc_periph_clock_enable(RCC_GPIOB);
    rcc_periph_clock_enable(RCC_SPI1);
    rcc_periph_clock_enable(RCC_SPI2);

    for (unsigned i = 0; i < NUM_COMBOS; i++) {
        if (test_combo(&combos[i])) {
            report_success(i + 1); /* never returns */
        }
    }

    report_failure(); /* never returns */
}
