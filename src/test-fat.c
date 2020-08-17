/*
  Copyright 2020 Harold Tay GPLv3
  Trace fat32 cluster chains.
 */

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/usart.h>
#include <libopencm3/stm32/gpio.h>
#include <string.h>
#include "tx.h"
#include "fil.h"
#include "power.h"
#include "usart_setup.h"
#define MHZ 48
#include "delay.h"

int
main(void)
{
  int8_t er;
  int16_t i;
  struct fil unused;

  rcc_clock_setup_in_hse_8mhz_out_48mhz();
  usart_setup(TX_USART, TX_PORT, TX_BIT, TX_AF, 57600);
  power_on(POWER_MODE_MASTER);

  for (i = 4; i > 0; i--) {
    tx_msg("Delay ", i);
    delay_ms(1000);
  }

  er = fil_init();
  tx_msg("fil_init returned ", er);
  while (er);

  /* Scan root directory and CATHK-0 */
  er = fil_scan_dbg();
  tx_msg("fil_scan_dbg returned ", er);
  while (er);
  er = fil_chdir("CATHK-0    ");
  tx_msg("fil_chdir returned ", er);
  while (er);
  er = fil_scan_dbg();
  tx_msg("fil_scan_dbg returned ", er);
  while (er);

  for ( ; ; ) ;
}
