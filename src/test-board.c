/*
  Copyright 2020 Harold Tay GPLv3
  Test board-in-progress with only MCU, serial port components
  installed.  No crystal.
 */

#include <libopencm3/cm3/scb.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/pwr.h>
#include <string.h>
#include "usart_setup.h"
#include "tx.h"
#define MHZ 8
#include "delay.h"

#include "sd2.h"
#include "attn.h"

int main(void)
{
  int8_t i, er;

  /* Use HSI, crystal probably not yet populated */
  /*
  Don't use PLL, this might be bricking the mcu!
  rcc_clock_setup_in_hsi_out_48mhz();
   */

  usart_setup(USART1, GPIOA, GPIO9, GPIO_AF1, 57600);
  attn_init();
  attn_on();

  /* Test cs led */
  sd_spi_init();
  sd_cs_lo();

  for (i = 4; i > 0; i--) {
    tx_msg("Delay ", i);
    delay_ms(900);
  }
  tx_puts("Turning off ATTN (yellow) led\r\n");
  attn_off();
  sd_cs_hi();
#if 0
  delay_ms(1000);
  er = fil_init();
  tx_msg("fil_init returned ", er);
  if (!er) attn_on();
#endif
  for ( ; ; ) ;

}
