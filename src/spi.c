#include "spi.h"

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/spi.h>
#include <libopencm3/stm32/dma.h>

/* SPI1: SCK = PA5, MISO = PA6 (unused), MOSI = PA7, AF5 */
static void spi1_gpio_setup(void)
{
    rcc_periph_clock_enable(RCC_GPIOA);

    gpio_mode_setup(GPIOA, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO5 | GPIO7);
    gpio_set_af(GPIOA, GPIO_AF5, GPIO5 | GPIO7);
    gpio_set_output_options(GPIOA, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO5 | GPIO7);
}

void spi1_setup(uint32_t baudrate_div)
{
    rcc_periph_clock_enable(RCC_SPI1);

    spi1_gpio_setup();

    spi_disable(SPI1);

    spi_init_master(
        SPI1,
        baudrate_div,
        SPI_CR1_CPOL_CLK_TO_1_WHEN_IDLE,
        SPI_CR1_CPHA_CLK_TRANSITION_1,
        SPI_CR1_DFF_8BIT,
        SPI_CR1_MSBFIRST
    );

    spi_enable(SPI1);
}

void spi1_write8(uint8_t data)
{
    spi_send(SPI1, data);
    spi_read(SPI1);
}

void spi1_write_buf(const uint8_t *buf, uint32_t len)
{
    for (uint32_t i = 0; i < len; i++) {
        spi1_write8(buf[i]);
    }
}

/* --- DMA bulk transfer (DMA2 Stream3 Channel3 = SPI1_TX) --- */

static int dma_ready = 0;

static void dma_init_once(void)
{
    if (dma_ready) return;
    dma_ready = 1;

    rcc_periph_clock_enable(RCC_DMA2);
}

void spi1_write_buf_dma(const uint8_t *buf, uint32_t len)
{
    dma_init_once();

    dma_stream_reset(DMA2, DMA_STREAM3);
    dma_channel_select(DMA2, DMA_STREAM3, DMA_SxCR_CHSEL_3);
    dma_set_priority(DMA2, DMA_STREAM3, DMA_SxCR_PL_HIGH);
    dma_set_transfer_mode(DMA2, DMA_STREAM3, DMA_SxCR_DIR_MEM_TO_PERIPHERAL);

    dma_enable_memory_increment_mode(DMA2, DMA_STREAM3);
    dma_disable_peripheral_increment_mode(DMA2, DMA_STREAM3);

    dma_set_peripheral_size(DMA2, DMA_STREAM3, DMA_SxCR_PSIZE_8BIT);
    dma_set_memory_size(DMA2, DMA_STREAM3, DMA_SxCR_MSIZE_8BIT);

    dma_set_peripheral_address(DMA2, DMA_STREAM3, (uint32_t)&SPI_DR(SPI1));
    dma_set_memory_address(DMA2, DMA_STREAM3, (uint32_t)buf);
    dma_set_number_of_data(DMA2, DMA_STREAM3, len);

    dma_clear_interrupt_flags(DMA2, DMA_STREAM3, DMA_TCIF);

    spi_enable_tx_dma(SPI1);
    dma_enable_stream(DMA2, DMA_STREAM3);

    while (!dma_get_interrupt_flag(DMA2, DMA_STREAM3, DMA_TCIF))
        ;

    dma_disable_stream(DMA2, DMA_STREAM3);
    spi_disable_tx_dma(SPI1);
}
