/*
  Copyright 2020 Harold Tay GPLv3
  Duplicates functionality of test-i2s.c to test pcm1808.{c,h}
 */

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/usart.h>

#include "pcm1808.h"
#include "usart_setup.h"
#include "tx.h"
#include "fmt.h"
#define MHZ 48
#include "delay.h"

#define POWER_RCC  RCC_GPIOB
#define POWER_PORT GPIOB
#define POWER_BIT  GPIO6

static void read_one_file(void)
{
  int8_t er;
  uint16_t tail, kb;

  kb = tail = 0;

  tx_puts("pcm1808_start\r\n");
  er = pcm1808_start();
  if (er) {
    tx_msg("pcm1808_start returned: ", er);
    for ( ; ; );
  }

  while (kb < 937) {                  /* should take 10s */
    /*tx_putc('!');*/
    if (tail == PCM1808_HEAD) {
      __asm__("nop");                 /* don't sync with sample rate! */
      __asm__("nop");                 /* don't sync with sample rate! */
      __asm__("nop");                 /* don't sync with sample rate! */
      continue;
    }
    tail++;
    if (0 == tail % 512) {
      tx_putc('#');                   /* got one sector */
      if (tail == PCM1808_BUFSZ) {
        tail = 0;
        kb++;
      }
    }
  }
  tx_puts("\r\n");
  pcm1808_stop();
}

int
main(void)
{
  int i;

  rcc_clock_setup_in_hse_8mhz_out_48mhz();
  rcc_periph_clock_enable(POWER_RCC);
  gpio_mode_setup(POWER_PORT, GPIO_MODE_OUTPUT,
    GPIO_PUPD_NONE, POWER_BIT);
  gpio_set(POWER_PORT, POWER_BIT);  /* turn it on, leave it on */
  usart_setup(USART1, GPIOA, GPIO9, GPIO_AF1, 57600);

  for (i = 4; i > 0; i--) {
    delay_ms(1000);
    tx_msg("Delay ", i);
  }

  for ( ; ; ) {
    read_one_file();
    delay_ms(2000);
  }
}
