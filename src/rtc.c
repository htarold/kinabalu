/*
  Copyright 2020 Harold Tay LGPLv3
  Code not specific to RTC chip.
 */
#include <libopencm3/stm32/i2c.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <string.h>
#include "rtc.h"
#include "i2c2.h"
#include "tx.h"
#include "fmt.h"

#define RTC_DBG
#ifdef RTC_DBG
#define dbg(x) x
#else
#define dbg(x) /* nothing */
#endif

char * rtc_print(struct rtc * rp)
{
  static char buf[24];
  static char * wk[] = {
    "",  "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
  strcpy(buf, "20");
  strcat(buf, fmt_x(rp->year & 0x3f));
  strcat(buf, "-");
  strcat(buf, fmt_x(rp->month & 0x1f));
  strcat(buf, "-");
  strcat(buf, fmt_x(rp->day_of_month & 0x3f));
  strcat(buf, "T");
  strcat(buf, fmt_x(rp->hours & 0x3f));
  strcat(buf, ":");
  strcat(buf, fmt_x(rp->minutes & 0x7f));
  strcat(buf, ":");
  strcat(buf, fmt_x(rp->seconds & 0x7f));
  strcat(buf, " ");
  if (rp->day_of_week <= 7)
    strcat(buf, wk[rp->day_of_week]);
  return(buf);
}

extern int8_t rtc_init(struct rtc * rp);

extern int8_t rtc_now(struct rtc * rp);

extern int8_t rtc_set_alarm(bool hourly);

extern int8_t rtc_alarm(struct rtc * rp);

/*
  After alarm has gone off, de-assert to allow it to fire again.
 */
extern int8_t rtc_rearm(void);

/*
  Interactively set the time.
 */
#include "usart_setup.h"              /* for usart_getc() */

static uint8_t getc(void)
{
  uint8_t ch;
  ch = usart_getc();
  tx_putc(ch);
  return(ch);
}
void rtc_user(struct rtc * rp)
{
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

  tx_puts("\r\nDay of week (1 digit, Sunday = 1): ");
  rp->day_of_week = getc() - '0';

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
  tx_puts(rtc_print(rp));
}
