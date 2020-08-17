/*
  Copyright 2020 Harold Tay GPLv3
  Simple test interfacing to SD card.
  Pin assignments:
  LED: PA4
  CS: PB1
  SPI1_SCK: PA5/AF0
  SPI1_MISO: PA6/AF0
  SPI1_MOSI: PA7/AF0
  USART1_TX: PA2/AF1
  USART1_RX: PA3/AF1
  I2C1_SDA: PA10/AF4
  I2C1_SCL: PA9/AF4

  PA0 and PA1 are free, can use for LRC and OUT.
  (PA13/14 used for programming, PF0/1 for crystal.  PA8/PB0
  not on F4P6.)

  XXX Not a good idea to use PA4/SPI1_NSS for LED.  Despite changing
  the pin function, the NSS function still seems active.  Best to
  not use.
 */

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/usart.h>
#include <libopencm3/stm32/spi.h>
#include <libopencm3/stm32/flash.h>
#include <libopencm3/stm32/f0/nvic.h>
#include <libopencm3/stm32/exti.h>
#include "tx.h"
#include "fmt.h"

#include "sd2.h"

#undef BB  /* use bit banged SPI */

#define LED_PORT GPIOA
#define LED_BIT  GPIO4

#define CS_PORT  GPIOB
#define CS_BIT   GPIO1

#define SCK_PORT GPIOA
#define SCK_BIT  GPIO5
#define SCK_AF   GPIO_AF0

#define MISO_PORT GPIOA
#define MISO_BIT  GPIO6
#define MISO_AF   GPIO_AF0

#define MOSI_PORT GPIOA
#define MOSI_BIT  GPIO7
#define MOSI_AF   GPIO_AF0

#define TX_PORT   GPIOA
#define TX_BIT    GPIO2
#define TX_AF     GPIO_AF1

#define RX_PORT   GPIOA
#define RX_BIT    GPIO3
#define RX_AF     GPIO_AF1

#define SDA_PORT  GPIOA
#define SDA_BIT   GPIO10
#define SDA_AF    GPIO_AF4

#define SCL_PORT  GPIOA
#define SCL_BIT   GPIO9
#define SCL_AF    GPIO_AF4

/*
  Not very accurate timing delays
 */
#define MHZ 40
#define delay_us(us) \
{ uint32_t i; for (i = 1.33*MHZ*us/8; i > 0; i--) { __asm__("nop"); } }

#define delay_ms(ms) \
{ uint32_t i; for (i = ms; i > 0; i--) { delay_us(1000); } }

static void rcc_clock_setup_in_hse_8mhz_out_40mhz(void)
{
        rcc_osc_on(RCC_HSE);
        rcc_wait_for_osc_ready(RCC_HSE);
        rcc_set_sysclk_source(RCC_HSE);

        rcc_set_hpre(RCC_CFGR_HPRE_NODIV);
        rcc_set_ppre(RCC_CFGR_PPRE_NODIV);

        flash_prefetch_enable();
        flash_set_ws(FLASH_ACR_LATENCY_024_048MHZ);

        /* PLL: 8MHz * 6 = 48MHz */
        rcc_set_pll_multiplication_factor(RCC_CFGR_PLLMUL_MUL5);
        rcc_set_pll_source(RCC_CFGR_PLLSRC_HSE_CLK);
        rcc_set_pllxtpre(RCC_CFGR_PLLXTPRE_HSE_CLK);

        rcc_osc_on(RCC_PLL);
        rcc_wait_for_osc_ready(RCC_PLL);
        rcc_set_sysclk_source(RCC_PLL);

        rcc_apb1_frequency = 40000000;
        rcc_ahb_frequency = 40000000;
}

/* LED is active low */
#define LED_OFF  GPIO_BSRR(LED_PORT) = LED_BIT
#define LED_ON   GPIO_BSRR(LED_PORT) = LED_BIT << 16
#define CS_HIGH  GPIO_BSRR(CS_PORT) = CS_BIT
#define CS_LOW   GPIO_BSRR(CS_PORT) = CS_BIT << 16

#define MOSI_HIGH GPIO_BSRR(MOSI_PORT) = MOSI_BIT
#define MOSI_LOW GPIO_BSRR(MOSI_PORT) = MOSI_BIT << 16
#define SCK_HIGH GPIO_BSRR(SCK_PORT) = SCK_BIT
#define SCK_LOW GPIO_BSRR(SCK_PORT) = SCK_BIT << 16

static void led_setup(void)
{
  rcc_periph_clock_enable(RCC_GPIOA);
  gpio_mode_setup(LED_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, LED_BIT);
  /* May not be the best place for this */
  rcc_periph_clock_enable(RCC_GPIOB);
  CS_HIGH;
  gpio_mode_setup(CS_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, CS_BIT);
}

/* at 200kHz clock, 1 tick = 5us, half tick = 2.5us */
#define bb_delay() delay_us(2)
#ifdef BB /* bit banged spi */
static void bb_setup(void)
{
  gpio_mode_setup(MOSI_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, MOSI_BIT);
  gpio_mode_setup(MISO_PORT, GPIO_MODE_INPUT, GPIO_PUPD_NONE, MISO_BIT);
  gpio_mode_setup(SCK_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, SCK_BIT);
}

static uint8_t bb_xfer(uint8_t x)
{
  int i;
  uint8_t in;
  in = 0;
  for (i = 0; i < 8; i++) {
    if (x & 0x80) {
      MOSI_HIGH;
    } else {
      MOSI_LOW;
    }
    bb_delay();
    SCK_HIGH;
    if (GPIO_IDR(MISO_PORT) & MISO_BIT)
      in |= 1;
    in <<= 1;
    bb_delay();
    SCK_LOW;
    x <<= 1;
  }
  return(in);
}

#else  /* using hardware SPI */
static void bb_setup(void)
{
  rcc_periph_clock_enable(RCC_SPI1);

  gpio_mode_setup(SCK_PORT, GPIO_MODE_AF, GPIO_PUPD_NONE, SCK_BIT);
  gpio_set_af(SCK_PORT, SCK_AF, SCK_BIT);

  gpio_mode_setup(MOSI_PORT, GPIO_MODE_AF, GPIO_PUPD_NONE, MOSI_BIT);
  gpio_set_af(MOSI_PORT, MOSI_AF, MOSI_BIT);

  gpio_mode_setup(MISO_PORT, GPIO_MODE_AF, GPIO_PUPD_NONE, MISO_BIT);
  gpio_set_af(MISO_PORT, MISO_AF, MISO_BIT);

  spi_set_unidirectional_mode(SPI1);
  spi_disable_crc(SPI1);
  spi_set_data_size(SPI1, SPI_CR2_DS_8BIT);
  spi_set_full_duplex_mode(SPI1);                            /* XXX? */
  spi_enable_software_slave_management(SPI1);
  spi_set_nss_high(SPI1);             /* Important! or SPI -> slave */
  spi_fifo_reception_threshold_8bit(SPI1);
  spi_set_baudrate_prescaler(SPI1, SPI_CR1_BR_FPCLK_DIV_128);  /* XXX? */
  spi_set_master_mode(SPI1);
  spi_send_msb_first(SPI1);
  spi_set_clock_polarity_0(SPI1);
  spi_set_clock_phase_0(SPI1);
  spi_enable_ss_output(SPI1);
  spi_enable(SPI1);
}
static uint8_t bb_xfer(uint8_t x)
{
  spi_send8(SPI1, x);
  return(spi_read8(SPI1));
}
#endif


static void usart_setup(void)
{
  rcc_periph_clock_enable(RCC_USART1);
  gpio_mode_setup(TX_PORT, GPIO_MODE_AF, GPIO_PUPD_NONE, TX_BIT);
  gpio_set_af(TX_PORT, TX_AF, TX_BIT);
  usart_set_baudrate(USART1, 57600);
  usart_set_databits(USART1, 8);
  usart_set_parity(USART1, USART_PARITY_NONE);
  usart_set_stopbits(USART1, USART_CR2_STOPBITS_1);
  usart_set_mode(USART1, USART_MODE_TX);
  usart_set_flow_control(USART1, USART_FLOWCONTROL_NONE);
  usart_enable(USART1);
}

void test_spi(void)
{
  int er, i;

  rcc_clock_setup_in_hse_8mhz_out_40mhz();
  led_setup();
  LED_OFF;                            /* Will cause NSS to fall */
  usart_setup();

  sd_delay(100);                      /* delay after power up */
  tx_puts("Starting\r\n");

  er = sd_spi_init();
  tx_msg("sd_spi_init = ", er);

  /* Provide 80 clocks without CS asserted.  */

  sd_cs_hi();
  sd_delay(10);
  for (i = 0; i < 10; i++) {
    (void)sd_xfer(0xff);              /* MOSI must be high */
  }

  tx_puts("80 clocks provided\r\n");

  /* Send CMD0 */
  sd_cs_lo();
  sd_xfer(0x40);
  sd_xfer(0x00);
  sd_xfer(0x00);
  sd_xfer(0x00);
  sd_xfer(0x00);
  sd_xfer(0x95);
  /* Wait for R1 response (single byte) */
  for (i = 0; i < 30000; i++) {
    er = sd_xfer(0xff);
    if (!(er & 0x80)) break;
    er = sd_xfer(0xff);
    if (!(er & 0x80)) break;
    er = sd_xfer(0xff);
    if (!(er & 0x80)) break;
    er = sd_xfer(0xff);
    if (!(er & 0x80)) break;
  }
  tx_msg("i = ", i);
  tx_msg("r1 = ", (uint8_t)er);
  sd_cs_hi();

  for ( ; ; ) ;

}

/*
  Timing:
  At 40MHz, approx 0.75us per loop count.
 */
#if 0
static void delay_ms(uint16_t ms)
{
  for ( ; ms > 0; ms--) {
    int i;
    for (i = 1336; i > 0; i--) {
      __asm__("nop");
    }
  }
}
#endif

int
main(void)
{
  int i;

  rcc_clock_setup_in_hse_8mhz_out_40mhz();
  led_setup();
  usart_setup();
  bb_setup();

  delay_ms(100);

  CS_HIGH;
  for (i = 0; i < 10; i++) {
    bb_xfer(0xff);
  }

#ifdef BB
  delay_us(20);
#else
  delay_us(130);
#endif
  CS_LOW;

  bb_xfer(0x40);
  bb_xfer(0x00);
  bb_xfer(0x00);
  bb_xfer(0x00);
  bb_xfer(0x00);
  bb_xfer(0x95);

  /*
    Returns with success after 2 bytes transferred.
    Clock is around 200kHz.
   */
  for ( ; ; ) {
    uint8_t in;
    in = bb_xfer(0xff);
    if (!(in & 0x80)) break;
  }

  for ( ; ; ) ;

}
