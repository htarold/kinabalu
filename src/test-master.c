/*
  Copyright 2020 Harold Tay GPLv3
  Test cfg with wkup and wav_record.
 */

#include <libopencm3/cm3/scb.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/pwr.h>
#include <string.h>
#include "usart_setup.h"
#include "wav.h"
#include "pcm1808.h"
#include "power.h"
#include "tx.h"
#include "fil.h"
#include "cfg.h"
#include "cfg_parse.h"
#include "wkup.h"
#include "tick.h"
#include "rtc.h"
#include "vdda.h"
#include "attn.h"
#include "bosch.h"
#define MHZ 48
#include "delay.h"

#include "sd2.h"

#define REALLY_SLEEP_DEEP

static int16_t vdda_mv;
static struct bosch bosch;

static int8_t read_sensors(void)
{
  int8_t er;

  /* Other sensors ... */
  vdda_init();
  delay_ms(1);
  vdda_mv = vdda_read_mv();
  cfg_log_attr("vdda", vdda_mv);

  if (!bosch.address)
    return(BOSCH_E_UNKNOWN_CHIP_ID);
  er = bosch_read_start(&bosch);
  cfg_log_attr("bosch_read_start", er);
  if (!er) {
#if 0
    int8_t i;
    for (i = 0; i < 100; i++)
      if (!(bosch_is_busy(&bosch))) break;
    if (i >= 100)
      cfg_log_attr("bosch_is_busy", 1);
#else
    delay_ms(80);
#endif
    er = bosch_read_end(&bosch);
    cfg_log_attr("bosch_read_end", er);
  }
  return(er);
}
static void record_sensors(void)
{
  cfg_log_lattr("bosch_temperature", bosch.temperature);
  cfg_log_ulattr("bosch_pressure", bosch.pressure);
  cfg_log_ulattr("bosch_humidity", (bosch.humidity*25)/256);
}

static int8_t add_mono(uint16_t lwm, uint16_t hwm)
{
  static uint16_t tmpbuf[64];
  uint8_t i;

  if (lwm & 0x1) lwm++;

  for (i = 0; lwm < hwm; ) {
    tmpbuf[i] = pcm1808_buf[lwm];
    lwm += 2;
    i++;
    if (i >= 64 || lwm >= hwm) {
      int8_t er;
      er = wav_add(tmpbuf, i);
      if (er) return(er);
      i = 0;
    }
  }
  return(0);
}
static int8_t record(struct rtc * rp, uint16_t seconds, bool mono)
{
  int8_t er;
  uint16_t lwm, hwm, count;
  char fn[12], lfn[27];

  tx_msg("record:mono=", mono);
  if (!read_sensors())
    record_sensors();
  wav_make_names(rp, cfg_sitename, *cfg_unit - '0', fn, lfn);
  cfg_log_lit("recording to:");
  cfg_logs(fn);
  cfg_logs(lfn);
  sd_buffer_sync();
  er = wav_record(rp, fn, lfn, seconds, mono?1:2);
  if (er) {
    cfg_log_attr("wav_record_er", er);
    if (FIL_EEXIST == er) {
      cfg_logs("File already exists, ignoring error.");
      er = 0;
    }
    goto fil_cleanup_return;
  }

  /* No write to SD card until recording ends (no logging allowed) */

  lwm = 0;
  er = pcm1808_start();
  if (er) {
    cfg_log_attr("pcm1808_start_er", er);
    goto cleanup_return;
  }

  for ( ; ; ) {
    hwm = PCM1808_HEAD;
    if (hwm < lwm)
      hwm = PCM1808_BUFSZ;
    count = hwm - lwm;
    if (mono) {
      er = add_mono(lwm, hwm);
    } else {
      er = wav_add(pcm1808_buf + lwm, count);
    }
    if (er) break;
    lwm += count;
    if (lwm == PCM1808_BUFSZ) lwm = 0;
  }
  if (1 == er) er = 0;                /* normal exit */
  cfg_log_attr("wav_add_error", er);

fil_cleanup_return:
  /* tx_msg("fil_reinit returned ", fil_reinit()); */
cleanup_return:
  pcm1808_stop();
  cfg_log_attr("record", er);
  cfg_log_attr("approx_sd_used_percent", fil_approx_sd_used_pct());
  if (!read_sensors())
    record_sensors();
  return(er);
}

static uint32_t rcc_csr;
static void clock_setup(void)
{
  rcc_csr = RCC_CSR;
  rcc_clock_setup_in_hse_8mhz_out_48mhz();
  usart_setup(USART1, GPIOA, GPIO9, GPIO_AF1, 57600);
  wkup_init();
  tick_init();
  power_on(POWER_MODE_MASTER);
  tx_puts("RCC_CSR=");
  tx_puthex32(rcc_csr);
  tx_puts("\r\n");
}

static void go_to_sleep(void)
{
  delay_ms(150);                      /* Wait for tx buffer to flush? XXX */
  sd_buffer_sync();                   /* prevent log corruption */
  cfg_log_sync();
  sd_buffer_checkout(SD_ADDRESS_NONE);
#ifdef REALLY_SLEEP_DEEP
  power_off();
  sd_spi_reset();                     /* GPIOs to input, 1.5mA XXX */
  sd_cs_hi();                         /* Turn off XXX */
  SCB_SCR |= SCB_SCR_SLEEPDEEP;
  SCB_SCR &= ~SCB_SCR_SLEEPONEXIT;
  pwr_set_stop_mode();
  pwr_voltage_regulator_low_power_in_stop();
  /* put tx pin in input mode to turn off LED.  */
  gpio_mode_setup(GPIOA, GPIO_MODE_INPUT,GPIO_PUPD_PULLUP, GPIO9);
#endif
  __asm__("wfi");
}

static void wake_up(void)
{
#ifdef REALLY_SLEEP_DEEP
  int8_t er, i;
  clock_setup();
  for (i = 0; i < 4; i++) {
    delay_ms(200);
    er = fil_reinit();
    if (!er) break;
    tx_msg("fil_reinit returned ", er);
  }
  cfg_log_attr("fil_reinit", er);
#endif
  cfg_log_attr("wkup_flag", wkup_flag);
  wkup_flag = 0;
  rtc_rearm();
}

static void safe_rtc_now(struct rtc * rp)
{
  int8_t er;
  while ((er = rtc_now(rp))) {
    cfg_log_attr("rtc_now_error", er);
    delay_ms(2000);
  }
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

  attn_init();
  need_sync = false;

  /*
    Initialisation section marked by attn_on()/attn_off().
   */
  attn_on();

  er = rtc_init(0);
  tx_msg("rtc_init returned ", er);
  if (er == RTC_NO_OSC) {
    tx_puts("Warning:rtc_init returned RTC_NO_OSC!\r\n");
    need_sync = true;
  } else if (er)
    while (er);

  er = fil_init();
  tx_msg("fil_init returned ", er);
  while (er);

  er = rtc_now(&now);
  tx_msg("rtc_now returned ", er);
  while (er);

  if (now.year < 19) need_sync = true;

  er = cfg_init(need_sync);
  while (er < 0) {
    tx_msg("cfg_init returned ", er);
    delay_ms(2000);
  }

  attn_off();

  cfg_log_lattr("rcc_csr", rcc_csr);  /* Find cause of reset */
  cfg_log_lattr("wav_sps", WAV_SPS);  /* compiled with sample rate? */

  er = bosch_init(&bosch, BOSCH_ADDR_PRI);
  if (er) er = bosch_init(&bosch, BOSCH_ADDR_SEC);
  if (er) bosch.address = 0;
  (void)read_sensors();               /* initial reading often erroneous */
  cfg_log_attr("bosch_init", er);

  {
    char dn[12];
    struct rtc now;

    for (i = 0; ; i++) {
      if (!cfg_sitename[i]) break;
      dn[i] = cfg_sitename[i];
    }

    dn[i++] = '-';
    dn[i++] = *cfg_unit;
    for ( ; i < sizeof(dn)-1; i++)
      dn[i] = ' ';
    dn[i] = '\0';

    er = fil_chdir(dn);
    tx_msg("fil_chdir returned ", er);
    if (er) {
      struct fil d;
      er = fil_mkdir(dn, 0, &d);
      CFG_PANIC((er != 0), "fil_mkdir_error", er);
      er = fil_fchdir(&d);
      CFG_PANIC((er != 0), "fil_fchdir_error", er);
    }

    er = fil_chdir(0);
    CFG_PANIC((er != 0), "fil_chdir_0_returned", er);

    cfg_logs("Recording deployment notes");
    er = rtc_now(&now);
    CFG_PANIC((er != 0), "rtc_now_error", er);
    cfg_log_attr("rtc_now_error", er);
    er = record(&now, 20, 1);
    CFG_PANIC((er != 0), "deployment_notes_record_error ", er);
    cfg_logs("Deployment notes recorded successfully");

    er = fil_chdir(dn);
    CFG_PANIC((er != 0), "fil_chdir_2_error", er);
  }

  rtc_rearm();
  wkup_enable();

  for ( ; ; ) {
    int16_t sleep_mins, duration;
    struct rtc alarm, now;
    uint8_t mono;

    /* Set alarm */
    safe_rtc_now(&now);
    alarm = now;
    sleep_mins = cfg_make_alarm(&alarm, &duration, &mono);
    /* if alarm is in the past, sleep to next minute */
    if (0 == sleep_mins)
      if (alarm.seconds /* == 0 */ <= now.seconds) {
        alarm.seconds = 0;
        alarm.hours = alarm.minutes =
        alarm.day_of_week = 0x80;
      }
    {
      char buf[30];
      strncpy(buf, rtc_print(&alarm), sizeof(buf)-1);
      buf[sizeof(buf)-1] = '\0';
      cfg_log_lit("Sleeping until:");
      cfg_logs(buf);
    }
    er = rtc_alarm(&alarm);
    if (er) {
      cfg_log_attr("rtc_alarm_error", er);
      delay_ms(2000);
      continue;
    }

    go_to_sleep();

    wake_up();

    /* Check time: record? */
    safe_rtc_now(&now);
    alarm = now;
    sleep_mins = cfg_make_alarm(&alarm, &duration, &mono);
    tx_msg("sleep_mins=", sleep_mins);
    tx_msg("now.seconds=", now.seconds);
    tx_msg("duration=", duration);
    tx_msg("mono=", mono);
    if (0 == sleep_mins               /* If too far into the minute, */
      && now.seconds < 3) {           /* assume woke up accidentally */
      cfg_log_lit("active");
      if (duration > 0) {
        er = record(&now, duration, mono);
        if (er == FIL_ENOSPC) break;
      }
    } else
      cfg_log_lit("resleep");
    if (vdda_mv && vdda_mv < 3100) {
      cfg_log_attr("battery_too_low", vdda_mv);
      break;
    }
  }

  cfg_log_lit("Stopping");
  sd_buffer_sync();
  for ( ; ; )
    go_to_sleep();
}
