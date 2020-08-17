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
#define MHZ 48
#include "delay.h"

#include "sd2.h"

static int8_t record(struct rtc * rp, uint16_t seconds)
{
  int8_t er;
  uint16_t lwm, hwm, count;
  char fn[12], lfn[27];

  cfg_log_lit("start recording");
  sd_buffer_sync();
  wav_make_names(rp, cfg_sitename, *cfg_unit - '0', fn, lfn);
  er = wav_record(rp, fn, lfn, seconds, 2);
  if (er) {
    cfg_log_attr("wav_record_er", er);
    if (FIL_EEXIST == er) {
      cfg_logs("File already exists, ignoring.");
      er = 0;
    }
    goto fil_cleanup_return;
  }
  /*
    Paint pcm1808_buf[] with something recognisable.
   */
  {
    char string[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                    "abcdefghijklmnopqrstuvwxyz"
                    "0123456789"
                    "\r\n";
    int i;
    for (i = 0; i < 1024; i += 64) {
      strcpy((void *)pcm1808_buf + i, string);
    }
  }

  /* No write to SD card until recording ends (no logging allowed) */
  lwm = 0;
#if 0
  er = pcm1808_start();
  if (er) {
    cfg_log_attr("pcm1808_start_er", er);
    goto cleanup_return;
  }
#endif
  for ( ; ; ) {
#if 0
    hwm = PCM1808_HEAD;
    if (hwm < lwm)
      hwm = PCM1808_BUFSZ;
    count = hwm - lwm;
    er = wav_add(pcm1808_buf + lwm, count);
#endif
    er = wav_add(pcm1808_buf, 1024);
    if (er) break;
    lwm += count;
    if (lwm == PCM1808_BUFSZ) lwm = 0;
  }
  if (1 == er) er = 0;  /* normal exit */

fil_cleanup_return:
  /* tx_msg("fil_reinit returned ", fil_reinit()); */
cleanup_return:
  pcm1808_stop();
  cfg_log_attr("record", er);
  cfg_log_attr("approx_sd_used_percent", fil_approx_sd_used_pct());
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

static void go_to_sleep(void) { }

/*
  Fake clock.
 */
int8_t rtc_init(struct rtc * unused) { return(0); }
static uint8_t tobcd(uint8_t d)
{
  uint8_t b;
  b = d % 10;
  b |= (d/10) << 4;
  return(b);
}
int8_t rtc_now(struct rtc * rp)
{
  static struct rtc r = { 0, 0, 0, 1, 1, 19, 0 };
  r.minutes++;
  if (r.minutes < 60) goto done;
  r.minutes = 0;
  r.hours++;
  if (r.hours < 24) goto done;
  r.hours = 0;
  r.day_of_month++;
  if (r.day_of_month < 31) goto done;
  r.day_of_month = 1;
  r.month++;
  if (r.month < 12) goto done;
  r.month = 1;
  r.year++;
done:
  rp->minutes = tobcd(r.minutes);
  rp->hours = tobcd(r.hours);
  rp->day_of_month = tobcd(r.day_of_month);
  rp->month = tobcd(r.month);
  rp->year = tobcd(r.year);
  return(0);
}
void rtc_user(struct rtc * unused) { return; }
char * rtc_print(struct rtc * unused) { return "(rtc_print)"; }

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

  need_sync = false;
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

  cfg_log_lattr("rcc_csr", rcc_csr);  /* Find cause of reset */

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
  }

  for ( ; ; ) {
    rtc_now(&now);
    er = record(&now, 90);
    if (er) break;
  }

  /* Cannot continue */
  tx_puts("Quitting\r\n");
  sd_buffer_sync();
  for ( ; ; )
    go_to_sleep();
}
