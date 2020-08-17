/*
  Copyright 2020 Harold Tay LGPLv3
  Arch-specific routines declared in sd2.h but defined here.
 */
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/spi.h>
#define MHZ 48
#include "delay.h"

#undef DBG
#ifdef DBG
#include "tx.h"
#include "fmt.h"
#define dbg(x) x
#else
#define dbg(x) /* nothing */
#endif

static int sd_spi_prescaler;
#define SD_SPI_PRESCALER_SLOW  SPI_CR1_BR_FPCLK_DIV_128
#define SD_SPI_PRESCALER_FAST  SPI_CR1_BR_FPCLK_DIV_4

#define SD_SPI   SPI2
#define SD_RCC   RCC_SPI2

#define CS_RCC   RCC_GPIOB
#define CS_PORT  GPIOB
#define CS_BIT   GPIO12

#define SCK_PORT GPIOB
#define SCK_BIT  GPIO13
#define SCK_AF   GPIO_AF0

#define MISO_PORT GPIOB
#define MISO_BIT  GPIO14
#define MISO_AF   GPIO_AF0

#define MOSI_PORT GPIOB
#define MOSI_BIT  GPIO15
#define MOSI_AF   GPIO_AF0

/* LED is active low */
#ifdef HAVE_LED
#define LED_OFF  GPIO_BSRR(LED_PORT) = LED_BIT
#define LED_ON   GPIO_BSRR(LED_PORT) = LED_BIT << 16
#endif
#define CS_HIGH  GPIO_BSRR(CS_PORT) = CS_BIT
#define CS_LOW   GPIO_BSRR(CS_PORT) = CS_BIT << 16

int8_t sd_spi_init(void)              /* used by sd2.c */
{
  rcc_periph_clock_enable(CS_RCC);
  gpio_mode_setup(CS_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_PULLUP, CS_BIT);
  CS_HIGH;
  /* spi_reset(SD_SPI); */
  rcc_periph_clock_enable(SD_RCC);

  gpio_mode_setup(SCK_PORT, GPIO_MODE_AF, GPIO_PUPD_NONE, SCK_BIT);
  gpio_set_af(SCK_PORT, SCK_AF, SCK_BIT);

  gpio_mode_setup(MOSI_PORT, GPIO_MODE_AF, GPIO_PUPD_NONE, MOSI_BIT);
  gpio_set_af(MOSI_PORT, MOSI_AF, MOSI_BIT);

  gpio_mode_setup(MISO_PORT, GPIO_MODE_AF, GPIO_PUPD_NONE, MISO_BIT);
  gpio_set_af(MISO_PORT, MISO_AF, MISO_BIT);

  spi_set_unidirectional_mode(SD_SPI);
  spi_disable_crc(SD_SPI);
  spi_set_data_size(SD_SPI, SPI_CR2_DS_8BIT);
  spi_set_full_duplex_mode(SD_SPI);                          /* XXX? */
  spi_enable_software_slave_management(SD_SPI);
  spi_set_nss_high(SD_SPI);             /* Important! or SPI -> slave */
  spi_set_baudrate_prescaler(SD_SPI,
    sd_spi_prescaler = SD_SPI_PRESCALER_SLOW);  /* XXX? */
  spi_set_master_mode(SD_SPI);
  spi_send_msb_first(SD_SPI);
  spi_set_clock_polarity_0(SD_SPI);
  spi_set_clock_phase_0(SD_SPI);
  spi_enable_ss_output(SD_SPI);
  spi_enable(SD_SPI);

  dbg(tx_puts("sd_spi_init ok\r\n"));
  return(0);
}

void sd_spi_reset(void)
{
  spi_reset(SD_SPI);
  gpio_mode_setup(CS_PORT,   GPIO_MODE_INPUT, GPIO_PUPD_NONE, CS_BIT);
  gpio_mode_setup(MOSI_PORT, GPIO_MODE_INPUT, GPIO_PUPD_NONE, MOSI_BIT);
  gpio_mode_setup(MISO_PORT, GPIO_MODE_INPUT, GPIO_PUPD_NONE, MISO_BIT);
  gpio_mode_setup(SCK_PORT,  GPIO_MODE_INPUT, GPIO_PUPD_NONE, SCK_BIT);
}

uint8_t sd_xfer(uint8_t x)
{
  uint16_t i, j;

  /* spi_send8() */
  j = ((sd_spi_prescaler==SD_SPI_PRESCALER_FAST)?1:32);
  for ( ; j > 0; j--) {
    for (i = 30000; i > 0; i--) {
      if (SPI_SR(SD_SPI) & SPI_SR_TXE) goto send;
      SPI_CR1(SD_SPI) |= SPI_CR1_MSTR;
      SPI_CR1(SD_SPI) |= SPI_CR1_SPE;
    }
  }
#ifdef DBG
  tx_puts("sd_xfer timed out waiting for SPI_SR_TXE, sr = ");
  tx_puthex32(SPI_SR(SD_SPI));
  tx_puts(" CR1 = ");
  tx_puthex32(SPI_CR1(SD_SPI));
  tx_puts(" CR2 = ");
  tx_puthex32(SPI_CR2(SD_SPI));
  tx_puts("\r\n");
#endif
  /* XXX What next? */
send:
  SPI_DR8(SD_SPI) = x;

  /* spi_read8() */
  /*
    XXX Can hang here, even after hard reset (but ok if
    reflashed).
  while (!(SPI_SR(SD_SPI) & SPI_SR_RXNE)) ;
   */
  return(SPI_DR8(SD_SPI));
}
void sd_cs_hi(void) { CS_HIGH; }
void sd_cs_lo(void) { CS_LOW; }
void sd_delay(uint8_t i)
{
  for ( ; i > 0; i--) { delay_us(250); }
}
void sd_go_fast(void)
{
  spi_set_baudrate_prescaler(SD_SPI,
    sd_spi_prescaler = SD_SPI_PRESCALER_FAST);  /* XXX? */
}

void dump_spi_regs(void)
{
#ifdef DBG
  tx_puts("SPI_CR1="); tx_puthex32(SPI_CR1(SD_SPI)); tx_puts("\r\n");
  tx_puts("SPI_CR2="); tx_puthex32(SPI_CR2(SD_SPI)); tx_puts("\r\n");
  tx_puts("SPI_SR="); tx_puthex32(SPI_SR(SD_SPI)); tx_puts("\r\n");
#endif
}
