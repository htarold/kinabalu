/*
  Copyright 2020 Harold Tay GPLv3
  Test filesystem using WRITE_MULTIPLE_BLOCKS
 */

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/usart.h>
#include <libopencm3/stm32/spi.h>
#include <libopencm3/stm32/flash.h>
#include <libopencm3/stm32/f0/nvic.h>
#include <libopencm3/stm32/exti.h>
#include "usart_setup.h"
#include "tx.h"
#include "fmt.h"

#include "wav.h"
#include "sd2.h"
#include "fil.h"
#define MHZ 48
#include "delay.h"                    /* spin delay */

#undef PROFILE
#ifdef PROFILE
#define P(x) /* nothing */
#else
#define P(x) x
#endif

#define POWER_RCC  RCC_GPIOB
#define POWER_PORT GPIOB
#define POWER_BIT  GPIO6

static void power_setup(void)
{
#ifdef POWER_RCC
  rcc_periph_clock_enable(POWER_RCC);
  gpio_mode_setup(POWER_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, POWER_BIT);
  gpio_set(POWER_PORT, POWER_BIT);
  tx_puts("POWER turned on\r\n");
#endif
}

static void msg32(char * s, uint32_t u)
{
  tx_puts(s); tx_puthex32(u); tx_puts("\r\n");
}

extern void dump_spi_regs(void);

int
main(void)
{
  int er, i;
  uint32_t address, nr_sectors;
  uint8_t line[] =
  "0123456789"
  "aAbBcCdDeEfFgGhHiIjJkKlLmMnNoOpPqQrRsStTuUvVwWxXyYzZ?\n"
  "0123456789"
  "aAbBcCdDeEfFgGhHiIjJkKlLmMnNoOpPqQrRsStTuUvVwWxXyYzZ?\n";
  uint32_t written;
  uint32_t cluster;

  rcc_clock_setup_in_hse_8mhz_out_48mhz();
  usart_setup(USART1, GPIOA, GPIO9, GPIO_AF1, 57600);
  power_setup();
  /* LED_ON;                            Will cause NSS to fall? */

  for (i = 8; i > 0; i--) {
    tx_msg("Delay ", i);
    delay_ms(800);
  }
  tx_puts("Starting\r\n");

  sd_delay(50);

  er = fil_init();
  tx_msg("fil_init returned: ", er);
  while (er) ;

  {
    struct rtc rtc;
    memset(&rtc, 0, sizeof(rtc));
    er = wav_record(&rtc, "SITEA", 0, 60, 2);
  }

  dump_spi_regs();
  while (er);

  tx_puts("Calling wav_add in a loop...\r\n");

  for ( ; ; ) {
    er = wav_add((void *)line, sizeof(line)/2);
    if (er) break;
  }

  tx_msg("wav_add returned ", er);
  if (1 == er)
    tx_puts("Success!\r\n");

  for ( ; ; );
}
