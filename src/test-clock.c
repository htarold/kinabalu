/*
  Copyright 2020 Harold Tay GPLv3
  Test RTC functions.
 */

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include "usart_setup.h"
#include "tx.h"
#include "rtc.h"
#include "power.h"
#define MHZ 48
#include "delay.h"

static char getc(void)
{
  char ch;
  ch = usart_recv_blocking(USART1);
  tx_putc(ch);
  return(ch);
}

static int8_t get_2_digits(void)
{
  int8_t n;
  n = (getc() - '0')<<4;
  n |= (getc() - '0');
  return(n);
}

static void get_time(struct rtc * rp)
{
  rp->day_of_month = 0x33;
  tx_puts("\r\nYear (2 digits): ");
  rp->year = get_2_digits();
  tx_puts("\r\nMonth (2 digits): ");
  rp->month = get_2_digits();
  tx_puts("\r\nDay of month (2 digits): ");
  rp->day_of_month = get_2_digits();
  tx_puts("\r\nHour (2 digits): ");
  rp->hours = get_2_digits();
  tx_puts("\r\nMinutes (2 digits): ");
  rp->minutes = get_2_digits();
  tx_puts("\r\nSeconds (2 digits): ");
  rp->seconds = get_2_digits();
  tx_puts("\r\nDay of week (1 digit, Sunday = 1): ");
  rp->day_of_week = getc() - '0';
}

static void clock_setup(void)
{
  rcc_clock_setup_in_hse_8mhz_out_48mhz();
  usart_setup(USART1, GPIOA, GPIO9, GPIO_AF1, 57600);
  power_on(POWER_MODE_MASTER);
}

int
main(void)
{
  int8_t i, er;
  struct rtc rtc;

  clock_setup();

  for (i = 4; i >= 0; i--) {
    tx_msg("Delay ", i);
    delay_ms(900);
  }

  er = rtc_init(0);
  tx_msg("rtc_init returned ", er);

  if (er) {
    get_time(&rtc);
    tx_puts("rtc.day_of_month = ");
    tx_puthex(rtc.day_of_month);
    tx_puts("\r\n");
    er = rtc_init(&rtc);
    tx_msg("rtc_init returned ", er);
    if (er) for ( ; ; );
  }

  for ( ; ; ) {
#if 1
    er = rtc_now(&rtc);
    if (er)
      tx_msg("rtc_now returned ", er);
    else
      tx_puts(rtc_print(&rtc));
#else
    er = rtc_init(0);
    tx_msg("rtc_init() returned ", er);
#endif
    delay_ms(1100);
  }
}
