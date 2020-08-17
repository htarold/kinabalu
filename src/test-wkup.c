/*
  Copyright 2020 Harold Tay GPLv3
  Test ds3231 module and i2c communications.
 */
#include <libopencm3/cm3/scb.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/f0/nvic.h>
#include <libopencm3/stm32/usart.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/pwr.h>

#include "usart_setup.h"
#include "wkup.h"
#include "rtc.h"
#include "tx.h"
#include "fmt.h"
#define MHZ 48
#include "delay.h"
#include "power.h"

#define GO_TO_SLEEP
#define RUN_RTC

static char getc(void)
{
  char ch;
  ch = usart_recv_blocking(USART1);
  tx_putc(ch);
  return(ch);
}

static void print_time(struct rtc * rp)
{
  tx_puthex(rp->hours); tx_putc(':');
  tx_puthex(rp->minutes); tx_putc(':');
  tx_puthex(rp->seconds); tx_puts("\r\n");
}

void read_user_time(struct rtc * rp)
{
  static char * dow[7] =
    { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };

  tx_puts("\r\nYear (2 digits): ");
  rp->year = getc() - '0';
  rp->year <<= 4; rp->year |= getc() - '0';

  tx_puts("\r\nMonth (2 digits): ");
  rp->month = getc() - '0';
  rp->month <<= 4; rp->month |= getc() - '0';

  tx_puts("\r\nDay (2 digits): ");
  rp->day_of_month = getc() - '0';
  rp->day_of_month <<= 4; rp->day_of_month |= getc() - '0';

  if (!rp->month && !rp->day_of_month) return;

  tx_puts("\r\nDay of week (1 digit): ");
  rp->day_of_week = getc() - '0';

  tx_puts("\r\n");
  tx_puthex(rp->year); tx_putc('/');
  tx_puthex(rp->month); tx_putc('/');
  tx_puthex(rp->day_of_month); tx_putc(' ');
  tx_puthex(rp->day_of_month); tx_putc(' ');
  tx_puts(dow[rp->day_of_week]); tx_puts("\r\n");

  tx_puts("\r\nHour (2 digits): ");
  rp->hours = getc() - '0';
  rp->hours <<= 4; rp->hours |= getc() - '0';
  tx_puts("\r\nMin (2 digits): ");
  rp->minutes = getc() - '0';
  rp->minutes <<= 4; rp->minutes |= getc() - '0';
  tx_puts("\r\nSec (2 digits): ");
  rp->seconds = getc() - '0';
  rp->seconds <<= 4; rp->seconds |= getc() - '0';

  tx_puts("\r\n");
  print_time(rp);
}

static void clock_setup(void)
{
  rcc_clock_setup_in_hse_8mhz_out_48mhz();
  usart_setup(USART1, GPIOA, GPIO9, GPIO_AF1, 57600);
  wkup_init();
  power_on(POWER_MODE_MASTER);
}

int
main(void)
{
  int i;
  int8_t er;
  struct rtc rtc;

  clock_setup();

  for (i = 4; i > 0; i--) {
    tx_msg("Delay ", i);
    delay_ms(950);
  }
#ifdef RUN_RTC
  er = rtc_init(0);
  tx_msg("rtc_init() returned ", er);
  if (!er) {
    print_time(&rtc);

    er = rtc_now(&rtc);
    tx_msg("rtc_now returned() ", er);
    while (er);
  }

  tx_puts("Change time? ");
  if ('y' == getc()) {
    read_user_time(&rtc);
    er = rtc_init(&rtc);
    tx_msg("rtc_init() returned ", er);
    while (er);
  }

  er = rtc_set_alarm(0);
  tx_msg("rtc_set_alarm() returned ", er);
  while (er);

  er = rtc_rearm();
  tx_msg("rtc_rearm() returned ", er);
  while (er);
#endif

  wkup_enable();
  delay_ms(20);
  tx_puts("Looping\r\n");

  for ( ; ; ) {
#ifdef GO_TO_SLEEP
    /*
      Page 87: set SLEEPDEEP; clear PDDS; select LPDS
     */
    delay_ms(100);
    SCB_SCR |= SCB_SCR_SLEEPDEEP;
    SCB_SCR &= ~SCB_SCR_SLEEPONEXIT;
    pwr_set_stop_mode();
    pwr_voltage_regulator_low_power_in_stop();
    power_off();
    __asm__("wfi");
    clock_setup();
    power_on(POWER_MODE_MASTER);
#else
    if (!wkup_flag) continue;
#endif
#ifdef RUN_RTC
    er = rtc_rearm();
    tx_puts("Called rtc_rearm\r\n");
#endif
    tx_msg("wkup_flag is ", wkup_flag);
    wkup_flag = 0;
  }
}
