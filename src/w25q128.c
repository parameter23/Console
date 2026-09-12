/**
 * @file w25q128.c
 * @brief Implementation of the W25Q128 SPI flash driver (see w25q128.h).
 */
#include "w25q128.h"
#include "spi.h"

#include <libopencm3/stm32/rcc.h>

#define CS_LOW()   gpio_clear(W25Q_CS_PORT, W25Q_CS_PIN)
#define CS_HIGH()  gpio_set(W25Q_CS_PORT, W25Q_CS_PIN)

#define CMD_WRITE_ENABLE   0x06
#define CMD_READ_STATUS1   0x05
#define CMD_PAGE_PROGRAM   0x02
#define CMD_SECTOR_ERASE   0x20
#define CMD_READ_DATA      0x03
#define CMD_READ_JEDEC_ID  0x9F

#define STATUS1_BUSY       0x01

/** @brief See w25q_init() in the header for the full contract. */
void w25q_init(void)
{
    rcc_periph_clock_enable(RCC_GPIOA);

    gpio_mode_setup(W25Q_CS_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, W25Q_CS_PIN);
    gpio_set_output_options(W25Q_CS_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, W25Q_CS_PIN);
    CS_HIGH();
}

/** @brief Sends a 24-bit address, MSB first (the format every W25Q128
 *         command below uses). */
static void send_addr(uint32_t addr)
{
    spi1_write8((uint8_t)(addr >> 16));
    spi1_write8((uint8_t)(addr >> 8));
    spi1_write8((uint8_t)addr);
}

/** @brief Busy-waits until the flash's internal write/erase cycle
 *         finishes (status register's BUSY bit clears). */
static void wait_ready(void)
{
    CS_LOW();
    spi1_write8(CMD_READ_STATUS1);
    uint8_t status;
    do {
        status = spi1_xfer8(0xFF);
    } while (status & STATUS1_BUSY);
    CS_HIGH();
}

/** @brief See w25q_read_id() in the header for the full contract. */
void w25q_read_id(uint8_t out[3])
{
    CS_LOW();
    spi1_write8(CMD_READ_JEDEC_ID);
    out[0] = spi1_xfer8(0xFF);
    out[1] = spi1_xfer8(0xFF);
    out[2] = spi1_xfer8(0xFF);
    CS_HIGH();
}

/** @brief See w25q_read() in the header for the full contract. */
void w25q_read(uint32_t addr, uint8_t *buf, uint32_t len)
{
    CS_LOW();
    spi1_write8(CMD_READ_DATA);
    send_addr(addr);
    spi1_read_buf(buf, len);
    CS_HIGH();
}

/** @brief See w25q_erase_sector() in the header for the full contract. */
void w25q_erase_sector(uint32_t addr)
{
    CS_LOW();
    spi1_write8(CMD_WRITE_ENABLE);
    CS_HIGH();

    CS_LOW();
    spi1_write8(CMD_SECTOR_ERASE);
    send_addr(addr);
    CS_HIGH();

    wait_ready();
}

/** @brief See w25q_write_page() in the header for the full contract. */
void w25q_write_page(uint32_t addr, const uint8_t *buf, uint32_t len)
{
    CS_LOW();
    spi1_write8(CMD_WRITE_ENABLE);
    CS_HIGH();

    CS_LOW();
    spi1_write8(CMD_PAGE_PROGRAM);
    send_addr(addr);
    spi1_write_buf(buf, len);
    CS_HIGH();

    wait_ready();
}
