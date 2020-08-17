/*
  Copyright 2020 Harold Tay GPLv3
  Test cfg with wkup.
 */

#include <libopencm3/cm3/scb.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/pwr.h>
#include "usart_setup.h"
#include "power.h"
#include "tx.h"
#include "fil.h"
#include "cfg.h"
#include "cfg_parse.h"
#include "wkup.h"
#include "tick.h"
#define MHZ 48
#include "delay.h"

static void clock_setup(void)
{
  rcc_clock_setup_in_hse_8mhz_out_48mhz();
  usart_setup(USART1, GPIOA, GPIO9, GPIO_AF1, 57600);
  wkup_init();
  tick_init();
  power_on(POWER_MODE_MASTER);
}

int main(void)
{
  int8_t i, er;
  bool need_sync;
  struct rtc now;

  clock_setup();
  for (i = 4; i > 0; i--) {
    tx_msg("Delay ", i);
    delay_ms(900);
  }
  tx_puts("Starting\r\n");

  need_sync = false;
  er = rtc_init(0);
  tx_msg("rtc_init returned ", er);
  if (er == RTC_NO_OSC) {
    tx_puts("Warning:rtc_init returned RTC_NO_OSC!\r\n");
    need_sync = true;
  } else
    while (er);

  er = fil_init();
  tx_msg("fil_init returned ", er);
  while (er);

  er = rtc_now(&now);
  tx_msg("rtc_now returned ", er);
  while (er);

  if (now.year < 19) need_sync = true;
  er = cfg_init(need_sync);
  tx_msg("cfg_init returned ", er);

  rtc_rearm();
  wkup_enable();

  for ( ; ; ) {
    int16_t minutes, duration;
    struct rtc alarm;
    uint8_t is_mono;
    er = rtc_now(&alarm);
    if (er) cfg_log_attr("rtc_now_returned", er);
    minutes = cfg_make_alarm(&alarm, &duration, &is_mono);
    cfg_log_attr("cfg_make_alarm", minutes);
    cfg_log_attr("duration_seconds", duration);

    if (0 == minutes) {
      cfg_log_lit("\"Recording\"");
      delay_ms(1100);
    }

    /* If recording was less than 1 minute, sleep until next minute */
    rtc_now(&now);
    if (now.minutes == alarm.minutes) {
      alarm.seconds = 0;
      alarm.minutes = 0x80;
      alarm.hours = 0x80;
      alarm.day_of_week = 0x80;
    }

    cfg_log_attr("alarm_seconds", alarm.seconds);
    cfg_log_attr("alarm_minutes", alarm.minutes);
    rtc_alarm(&alarm);
    cfg_log_lit("Going to sleep");
    delay_ms(200);  /* Wait for tx buffer to flush? XXX */

    SCB_SCR |= SCB_SCR_SLEEPDEEP;
    SCB_SCR &= ~SCB_SCR_SLEEPONEXIT;
    pwr_set_stop_mode();
    pwr_voltage_regulator_low_power_in_stop();
    power_off();
    /* put tx pin in input mode to turn off LED.  */
    gpio_mode_setup(GPIOA, GPIO_MODE_INPUT,GPIO_PUPD_PULLUP, GPIO9);
    __asm__("wfi");

    clock_setup();
    cfg_log_lit("waking up");
    cfg_log_attr("wkup_flag", wkup_flag);
    wkup_flag = 0;
    rtc_rearm();
  }
}
