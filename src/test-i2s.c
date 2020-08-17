/*
  Copyright 2020 Harold Tay GPLv3
  Test I2S.
 */

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/usart.h>
#include <libopencm3/stm32/spi.h>
#include <libopencm3/stm32/flash.h>
#include <libopencm3/stm32/f0/nvic.h>
#include <libopencm3/stm32/exti.h>
#include <libopencm3/stm32/f0/spi.h>
#include <libopencm3/stm32/dma.h>
#include "tx.h"
#include "fmt.h"
#include "sd2.h"

#define MHZ 48
#include "delay.h"                    /* spin delay */

#define LED_PORT     GPIOC
#define LED_BIT      GPIO13
#define LED_RCC      RCC_GPIOC

#define TX_RCC       RCC_USART1
#define TX_PORT      GPIOA
#define TX_BIT       GPIO9
#define TX_AF        GPIO_AF1

#define RX_RCC       RCC_USART1
#define RX_PORT      GPIOA
#define RX_BIT       GPIO10
#define RX_AF        GPIO_AF1

#define I2S_SPI      SPI1
#define I2S_RCC      RCC_SPI1

#define I2S_WS_PORT  GPIOA
#define I2S_WS_BIT   GPIO15
#define I2S_WS_AF    GPIO_AF0

#define I2S_CK_PORT  GPIOB
#define I2S_CK_BIT   GPIO3
#define I2S_CK_AF    GPIO_AF0

#define I2S_MCK_PORT GPIOB
#define I2S_MCK_BIT  GPIO5
#define I2S_MCK_AF   GPIO_AF0

#define I2S_SD_PORT  GPIOB
#define I2S_SD_BIT   GPIO5
#define I2S_SD_AF    GPIO_AF0

#define I2S_WS2_PORT GPIOB
#define I2S_WS2_BIT  GPIO4

#define SD_SPI       SPI2
#define SD_SPI_RCC   RCC_SPI2

#define SD_CS_PORT   GPIOB
#define SD_CS_BIT    GPIO12
#define SD_CS_RCC    RCC_GPIOB

#define SD_SCK_PORT  GPIOB
#define SD_SCK_BIT   GPIO13
#define SD_SCK_AF    GPIO_AF0

#define SD_MISO_PORT GPIOB
#define SD_MISO_BIT  GPIO14
#define SD_MISO_AF   GPIO_AF0

#define SD_MOSI_PORT GPIOB
#define SD_MOSI_BIT  GPIO15
#define SD_MOSI_AF   GPIO_AF0

#define SETUP(port,bit,af,pupd) \
gpio_mode_setup(port, GPIO_MODE_AF, pupd, bit); \
gpio_set_af(port, af, bit);

static void rcc_setup(void)
{
  rcc_clock_setup_in_hse_8mhz_out_48mhz();
#if 1  /* for USART etc. Or, where to put this? XXX */
  rcc_periph_clock_enable(RCC_GPIOA);
  rcc_periph_clock_enable(RCC_GPIOB);
#endif
}

static void usart_setup(void)
{
  rcc_periph_clock_enable(RCC_USART1);
  SETUP(TX_PORT, TX_BIT, TX_AF, GPIO_PUPD_NONE)
  SETUP(RX_PORT, RX_BIT, RX_AF, GPIO_PUPD_NONE)
  usart_set_baudrate(USART1, 57600);
  usart_set_databits(USART1, 8);
  usart_set_parity(USART1, USART_PARITY_NONE);
  usart_set_stopbits(USART1, USART_CR2_STOPBITS_1);
  usart_set_mode(USART1, USART_MODE_TX_RX);
  usart_set_flow_control(USART1, USART_FLOWCONTROL_NONE);
  usart_enable(USART1);
}

/*
  At 48ksps, circbuf[1024] (2k of memory!) will be filled in 10.7ms,
  should be enough of a buffer.
 */
#define CIRCBUF_SIZE 1024
uint16_t circbuf[CIRCBUF_SIZE];
uint16_t circbuf_head;                /* Consume from here */
/*
  CNDTR contains the number of data yet to be transferred, so
  CIRCBUF_TAIL points to where the NEXT data will go.
 */
#define CIRCBUF_TAIL (CIRCBUF_SIZE-DMA_CNDTR(DMA1, DMA_CHANNEL2))

/*
  DMA setup XXX
  Using DMA only for I2S reception.
  I2S (=SPI1) rx can use channel 2.
  Configuration procedure (pg 191):
 */
static void dma_setup(void)
{
  rcc_periph_clock_enable(RCC_DMA1);
  dma_channel_reset(DMA1, DMA_CHANNEL2); /* Must use ch 2 */
  dma_set_peripheral_address(DMA1, DMA_CHANNEL2, (uint32_t)&SPI1_DR);
  dma_set_memory_address(DMA1, DMA_CHANNEL2, (uint32_t)circbuf);
  dma_set_number_of_data(DMA1, DMA_CHANNEL2, CIRCBUF_SIZE);
  dma_enable_circular_mode(DMA1, DMA_CHANNEL2);
  dma_set_read_from_peripheral(DMA1, DMA_CHANNEL2);
  dma_enable_memory_increment_mode(DMA1, DMA_CHANNEL2);
  dma_set_peripheral_size(DMA1, DMA_CHANNEL2, DMA_CCR_PSIZE_16BIT);
  dma_set_memory_size(DMA1, DMA_CHANNEL2, DMA_CCR_MSIZE_16BIT);
  dma_set_priority(DMA1, DMA_CHANNEL2, DMA_CCR_PL_VERY_HIGH);
  dma_enable_channel(DMA1, DMA_CHANNEL2);

  /* DMA_CNDTR2 contains the number of data remaining */
  circbuf_head = 0;
}

static void i2s_setup(void)
{
  /*
    force I2S to shut down first. XXX
   */
  while (SPI_SR(I2S_SPI) & (1<<7)) ;   /* BSY flag */
  SPI_I2SCFGR(I2S_SPI) &= ~(1<<10);
  delay_ms(1);

  dma_setup();

  rcc_periph_clock_enable(I2S_RCC);
  SETUP(I2S_WS_PORT, I2S_WS_BIT, I2S_WS_AF, GPIO_PUPD_NONE)
  SETUP(I2S_CK_PORT, I2S_CK_BIT, I2S_CK_AF, GPIO_PUPD_NONE)
  SETUP(I2S_SD_PORT, I2S_SD_BIT, I2S_SD_AF, GPIO_PUPD_NONE)
  gpio_mode_setup(I2S_WS2_PORT, GPIO_MODE_INPUT, GPIO_PUPD_NONE, I2S_WS2_BIT);
  /*
    I2S will be half-duplex, slave (PCM1808 provides the clocks).
    Pg 785, "I2S Philips protocol waveform" confirms to PCM1808
    pg. 17 FMT is low.
    I2S must be active and enabled before WS becomes active:
    I2SE must be set to 1 when WS = 1.
    See pg. 797, section 28.7.7.
   */
  /* Set I2SMOD; choose I2SSTD, DATLEN, CHLEN, I2SCFG */
  /* SPI_CR1(SPI1) not used in I2S mode */
  /* SPI_CR2(SPI1) is used for interrupt and DMA flags */
  /*
    SPI_SR(SPI1) is used for frame error, busy, overrun,
    underrun, CHSIDE, TXE, RXNE
   */
  spi_enable_rx_dma(I2S_SPI);
  SPI_I2SCFGR(I2S_SPI) =
                /* settings for PCM1808, FMT = MD1 = 0, MD0 = 1 */
      (1<<11)   /* I2SMOD = I2S mode */
    | (1<<8)    /* I2SCFG = slave receive */
    | (0<<4)    /* I2SSTD = philips */
    | (0<<3)    /* CKPOL = 0? XXX works if 1 or 0. */
    | (0<<1)    /* DATLEN = 16 bits */
    | (1<<0)    /* CHLEN = 32 bits */
    ;

  /*
    Avoid frame errors, wait until WS rises.
   */
#define WS2_HIGH (GPIO_IDR(I2S_WS2_PORT) & I2S_WS2_BIT)
#define WS2_LOW (!(GPIO_IDR(I2S_WS2_PORT) & I2S_WS2_BIT))
  while (WS2_HIGH) ;
  while (WS2_LOW) ;
  SPI_I2SCFGR(I2S_SPI) |=
      (1<<10);  /* I2S enable, after other bits configed */
}


int8_t sd_spi_init(void)
{
  gpio_mode_setup(SD_CS_PORT, GPIO_MODE_OUTPUT,
    GPIO_PUPD_NONE, SD_CS_BIT);
  gpio_set(SD_CS_PORT, SD_CS_BIT);
  rcc_periph_clock_enable(SD_SPI_RCC);
  SETUP(SD_SCK_PORT, SD_SCK_BIT, SD_SCK_AF, GPIO_PUPD_NONE);
  SETUP(SD_MISO_PORT, SD_MISO_BIT, SD_MISO_AF, GPIO_PUPD_NONE);
  SETUP(SD_MOSI_PORT, SD_MOSI_BIT, SD_MOSI_AF, GPIO_PUPD_NONE);

  spi_set_unidirectional_mode(SD_SPI);
  spi_disable_crc(SD_SPI);
  spi_set_data_size(SD_SPI, SPI_CR2_DS_8BIT);   /* XXX change to 16? */
  spi_set_full_duplex_mode(SD_SPI);
  spi_enable_software_slave_management(SD_SPI);
  spi_set_nss_high(SD_SPI);
  spi_set_baudrate_prescaler(SD_SPI, SPI_CR1_BR_FPCLK_DIV_128);
  spi_set_master_mode(SD_SPI);
  spi_send_msb_first(SD_SPI);
  spi_set_clock_polarity_0(SD_SPI);
  spi_set_clock_phase_0(SD_SPI);
  spi_enable_ss_output(SD_SPI);
  spi_enable(SD_SPI);
  return(0);
}
void sd_go_fast(void)
{
  spi_set_baudrate_prescaler(SD_SPI, SPI_CR1_BR_FPCLK_DIV_4); /* XXX */
}
void sd_cs_hi(void) { GPIO_BSRR(SD_CS_PORT) = SD_CS_BIT; }
void sd_cs_lo(void) { GPIO_BSRR(SD_CS_PORT) = (SD_CS_BIT << 16); }
/* Only used for file operations, not multi block writes */
uint8_t sd_xfer(uint8_t x)
{
  while (!(SPI_SR(SD_SPI) & SPI_SR_TXE)) ;
  SPI_DR8(SD_SPI) = x;
  return(SPI_DR8(SD_SPI));
}

int
main(void)
{
  int i;
  uint16_t v;

  rcc_setup();
  usart_setup();
  for (i = 4; i > 0; i--) {
    delay_ms(800);
    tx_msg("Delay ", i);
  }
restart:
  i2s_setup();
  tx_puts("i2s setup\r\n");

  /*
    XXX clear ovr flag?
   */
#if 0
  v = SPI_DR(I2S_SPI); 
  v = SPI_SR(I2S_SPI);
  for ( ; ; ) {
    uint16_t sr;
    for ( ; ; ) {
      sr = SPI_SR(I2S_SPI);
      if (sr & SPI_SR_RXNE) break;
      if (sr & SPI_SR_OVR) break;
    }
    v = SPI_DR(I2S_SPI);
    tx_puthex32(v);
    tx_puts("\r\n");
  }
#else
  v = 0;
  for ( ; ; ) {
    if (SPI_SR(I2S_SPI) & (1<<8)) {   /* frame error */
      tx_puts("F ");
      goto restart;
    }
    if (SPI_SR(I2S_SPI) & SPI_SR_OVR) {
      /* clear the error */
      uint16_t tmp;
      tmp = SPI_DR(I2S_SPI);
      tmp = SPI_SR(I2S_SPI);
      tx_puts("O ");
      goto restart;
    }
#if 0
    if (CIRCBUF_TAIL > circbuf_head) {
      tx_putdec(circbuf_head); tx_putc(':');
      tx_putdec(circbuf[circbuf_head++]); tx_puts("\r\n");
    } else
      tx_puts("wait ");
#else
    if ((!v) && (CIRCBUF_TAIL >= (CIRCBUF_SIZE-2))) {
      tx_puts("#");
      v = 1;
    } else {
      v = 0;
    }
#endif
  }
#endif
}
