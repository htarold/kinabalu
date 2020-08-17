/*
  Copyright 2020 Harold Tay GPLv3
  Test cfg with wkup.
 */

#include <libopencm3/cm3/scb.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/pwr.h>
#include "usart_setup.h"
#include "tx.h"
#include "tick.h"
#define MHZ 48
#include "delay.h"

static void clock_setup(void)
{
  rcc_clock_setup_in_hse_8mhz_out_48mhz();
  usart_setup(USART1, GPIOA, GPIO9, GPIO_AF1, 57600);
}

int main(void)
{
  int8_t i;

  clock_setup();
  for (i = 4; i > 0; i--) {
    tx_msg("Delay ", i);
    delay_ms(900);
  }
  tick_init();
  for ( ; ; ) {
    delay_ms(100);
    if (tick_need_stamp())
      tx_putc('Y');
    else
     tx_putc('n');
  }
}
