/*
  Copyright 2020 Harold Tay LGPLv3
  Assumes PCM1808 interfaced to stm32f051c8t6.
  Data are copied by DMA to a circular memory buffer.
 */

#include <libopencm3/stm32/dma.h>
#include <libopencm3/stm32/spi.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include "pcm1808.h"
#ifndef MHZ
#define MHZ 48
#endif
#include "delay.h"

uint16_t pcm1808_buf[PCM1808_BUFSZ];

int8_t pcm1808_start(void)
{
  int i;


  /* Force I2S to shut down first. */

  for (i = 300; SPI_SR(I2S_SPI) & SPI_SR_BSY; i--)
    if (!i) return(PCM1808_SR_BUSY);
  SPI_I2SCFGR(I2S_SPI) &= ~SPI_I2SCFGR_I2SE;
  delay_us(50);                       /* wait for shut down */

  /*
    Set up DMA
   */
  rcc_periph_clock_enable(I2S_DMA_RCC);
#define WHICH I2S_DMA, I2S_CHANNEL
  dma_channel_reset(WHICH);
  dma_set_peripheral_address(WHICH, (uint32_t)&(SPI_DR(I2S_SPI)));
  dma_set_memory_address(WHICH, (uint32_t)pcm1808_buf);
  dma_set_number_of_data(WHICH, PCM1808_BUFSZ);
  dma_enable_circular_mode(WHICH);
  dma_set_read_from_peripheral(WHICH);
  dma_enable_memory_increment_mode(WHICH);
  dma_set_peripheral_size(WHICH, DMA_CCR_PSIZE_16BIT);
  dma_set_memory_size(WHICH, DMA_CCR_MSIZE_16BIT);
  dma_set_priority(WHICH, DMA_CCR_PL_VERY_HIGH);
  dma_enable_channel(WHICH);

  rcc_periph_clock_enable(I2S_RCC);

#define SETUP(port, bit, af, rcc) \
gpio_mode_setup(port,GPIO_MODE_AF,GPIO_PUPD_NONE,bit);\
gpio_set_af(port,af,bit); \
rcc_periph_clock_enable(rcc);
  SETUP(I2S_WS_PORT, I2S_WS_BIT, I2S_WS_AF, I2S_WS_RCC);
  SETUP(I2S_CK_PORT, I2S_CK_BIT, I2S_CK_AF, I2S_CK_RCC);
  SETUP(I2S_SD_PORT, I2S_SD_BIT, I2S_SD_AF, I2S_SD_RCC);
  /* Tied to WS pin, so we can read the WS state: */
  gpio_mode_setup(
    I2S_WS2_PORT, GPIO_MODE_INPUT, GPIO_PUPD_NONE, I2S_WS2_BIT);

  spi_enable_rx_dma(I2S_SPI);
  SPI_I2SCFGR(I2S_SPI) = 
      SPI_I2SCFGR_I2SMOD
    | (SPI_I2SCFGR_I2SCFG_SLAVE_RECEIVE << SPI_I2SCFGR_I2SCFG_LSB)
    | (SPI_I2SCFGR_I2SSTD_I2S_PHILIPS << SPI_I2SCFGR_I2SSTD_LSB)
    /* | SPI_I2SCFGR_CKPOL works with polarity = 0 */
    | (SPI_I2SCFGR_DATLEN_16BIT << SPI_I2SCFGR_DATLEN_LSB)
    | SPI_I2SCFGR_CHLEN;
  /*
    Start I2S when WS is high.
   */
  for (i = 5000; (GPIO_IDR(I2S_WS2_PORT) & I2S_WS2_BIT); i--)
    if (!i) return(PCM1808_WS2_HIGH);
  for (i = 5000; !(GPIO_IDR(I2S_WS2_PORT) & I2S_WS2_BIT); i--)
    if (!i) return(PCM1808_WS2_LOW);

  /* Enable I2S when WS is high */
  SPI_I2SCFGR(I2S_SPI) |= SPI_I2SCFGR_I2SE;
  return(0);
}

void pcm1808_stop(void)
{
  int i;
  SPI_I2SCFGR(I2S_SPI) &= ~SPI_I2SCFGR_I2SE;
  for (i = 300; i > 0 && SPI_SR(I2S_SPI) & SPI_SR_BSY; i--);
  dma_channel_reset(WHICH);
}
