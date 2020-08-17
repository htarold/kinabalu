/*
  Copyright 2020 Harold Tay GPLv3
  Test bosch.c in a loop.
 */
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/i2c.h>
#include <libopencm3/stm32/pwr.h>
#include <string.h>
#include "bosch.h"
#include "usart_setup.h"
#include "power.h"
#include "tx.h"
#include "fmt.h"
#include "rtc.h"
#define MHZ 48
#include "delay.h"

/*
  Overrides standard cfg_log_*() functions.
 */
void cfg_log_attr(char * name, int16_t val)
{
  tx_puts(name);
  tx_putc('=');
  tx_putdec(val);
  tx_putc(' ');
}
void cfg_log_lattr(char * name, int32_t val)
{
  tx_puts(name);
  tx_putc('=');
  tx_putdec32(val);
  tx_putc(' ');
}
void cfg_log_ulattr(char * name, uint32_t val)
{
  tx_puts(name);
  tx_putc('=');

  tx_puts(fmt_u32d(val));
  tx_putc(' ');
}

static void clock_setup(void)
{
  rcc_clock_setup_in_hse_8mhz_out_48mhz();
  usart_setup(USART1, GPIOA, GPIO9, GPIO_AF1, 57600);
  power_on(POWER_MODE_MASTER);
}

static void report(char * s, char * val)
{
  tx_puts(s);
  tx_puts(val);
  tx_puts("\r\n");
}

int main(void)
{
  int8_t i;
  struct bosch bosch;

  clock_setup();
  for (i = 4; i >= 0; i--) {
    tx_msg("Delay ", i);
    delay_ms(800);
  }
  tx_puts("Starting\r\n");

  if (rtc_i2c_init(I2C2, GPIOB, GPIO10 | GPIO11, GPIO_AF1))
    return(RTC_I2C_ERROR);

  tx_puts("Calling bosch_init...");

  i = bosch_init(&bosch, BOSCH_ADDR_PRI);
  tx_msg("bosch_init returned ", i);
  if (i) {
    i = bosch_init(&bosch, BOSCH_ADDR_SEC);
    tx_msg("bosch_init returned ", i);
  }
  while (i) ;

  tx_msg("chip_id is ", bosch.chip_id);
  if (BOSCH_IS_BME(&bosch))
    tx_puts("Is BME\r\n");
  else
    tx_puts("Is BMP\r\n");

  for ( ; ; ) {
    int8_t er;
    tx_puts("Calling bosch_read_start()\r\n");
    i = bosch_read_start(&bosch);
    tx_msg("bosch_read_start returned ", i);
    while (i);

    while ((er = bosch_is_busy(&bosch)))
      tx_msg("bosch_is_busy returned ", er);

    tx_puts("Calling bosch_read_end()\r\n");
    i = bosch_read_end(&bosch);
    tx_msg("bosch_read_end returned ", i);
    while (i);

    report("Temperature:", fmt_i32d(bosch.temperature));
    report("Pressure:", fmt_u32d(bosch.pressure));
    report("Humidity:", fmt_u32d(bosch.humidity));
  }
}
