/*
  Copyright 2020 Harold Tay LGPLv3
  RTC functions specific to DS3231.
 */
#include <libopencm3/stm32/i2c.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <string.h>
#include "rtc.h"
#include "i2c2.h"
#include "fil.h"                      /* For FIL_DATE, FIL_TIME */
#include "tx.h"
#include "fmt.h"

#define DS3231_I2C          I2C2

#define DS3231_SDA_SCL_PORT GPIOB
#define DS3231_SDA_BIT      GPIO11
#define DS3231_SDA_SCL_AF   GPIO_AF1
#define DS3231_SCL_BIT      GPIO10

/*
  Device addresses
 */
#define DS3231_ADDR7BIT 0x68
#define DS3231_CURSECS  0x00
#define DS3231_CURMINS  0x01
#define DS3231_CURHOURS 0x02
#define DS3231_CURDOW   0x03
#define DS3231_CURDATE  0x04
#define DS3231_CURMNTH  0x05  /* MSB is century bit */
#define DS3231_CURYEAR  0x06
#define DS3231_A1BASE   0x08
#define DS3231_A2BASE   0x0B
/* For alarms, MSB is mask bit. */
#define DS3231_AOFFSECS (-1) /* Alarm 2 does not have this field */
#define DS3231_AOFFMINS  0
#define DS3231_AOFFHRS   1
#define DS3231_AOFFDAY   2
#define DS3231_CONTROL  0x0E
#define DS3231_STATUS   0x0F
#define DS3231_AGING    0x10
#define DS3231_TMPMSB   0x11
#define DS3231_TMPLSB   0x12

#define RTC_DBG
#ifdef RTC_DBG
#define dbg(x) x
#else
#define dbg(x) /* nothing */
#endif

extern char * rtc_print(struct rtc * rp);

int8_t rtc_init(struct rtc * rp)
{
  uint8_t res, cmd[8];

  if (rtc_i2c_init(DS3231_I2C, DS3231_SDA_SCL_PORT,
    DS3231_SDA_BIT | DS3231_SCL_BIT, DS3231_SDA_SCL_AF))
    return(RTC_I2C_ERROR);

  *cmd = DS3231_STATUS;               /* status register */
  res = 0xa5;
  dbg(tx_puts("rtc_init i2c2_transfer7... "));
  if (i2c2_transfer7(DS3231_I2C, DS3231_ADDR7BIT, cmd, 1, &res, 1))
    return(RTC_I2C_ERROR);
  dbg(tx_puts("rtc_init i2c2_transfer7 done\r\n"));
  if (res == 0xa5)
    return(RTC_I2C_ERROR);

  dbg(tx_puts("status register: "));
  dbg(tx_puthex(res));
  dbg(tx_puts("\r\n"));
  if (!rp)
    return(res&0x80?RTC_NO_OSC:0);

  /*
    Set date, time.
   */
  cmd[0] = 0;
  cmd[1] = rp->seconds & 0x7f;
  cmd[2] = rp->minutes & 0x7f;
  cmd[3] = rp->hours & 0x3f;          /* hours & am/pm */
  cmd[4] = rp->day_of_week & 0x07;
  cmd[5] = rp->day_of_month & 0x3f;
  cmd[6] = rp->month & 0x1f;
  cmd[7] = rp->year & 0xff;

  if (i2c2_transfer7(DS3231_I2C, DS3231_ADDR7BIT, cmd, 8, 0, 0))
    return(RTC_I2C_ERROR);

  cmd[0] = DS3231_STATUS;
#if 0
  cmd[1] = 0;                         /* turn off 32kHz output, OSF */
#else
  cmd[1] = (1<<2);
#endif
  if (i2c2_transfer7(DS3231_I2C, DS3231_ADDR7BIT, cmd, 2, 0, 0))
    return(RTC_I2C_ERROR);
  return(0);
}

int8_t rtc_now(struct rtc * rp)
{
  uint8_t cmd, res[7];

  cmd = 0;                            /* start address of time/date */
  if (i2c2_transfer7(DS3231_I2C, DS3231_ADDR7BIT, &cmd, 1, res, 0))
    return(RTC_I2C_ERROR);
  if (i2c2_transfer7(DS3231_I2C, DS3231_ADDR7BIT, &cmd, 0, res, 7))
    return(RTC_I2C_ERROR);
  rp->seconds = res[0];
  rp->minutes = res[1];
  rp->hours = res[2];
  rp->day_of_week = res[3];
  rp->day_of_month = res[4];
  rp->month = res[5];
  rp->year = res[6];
#if 0
  dbg(tx_puts("rtc_now got: "));
  dbg(tx_puts(rtc_print(rp)));
  dbg(tx_puts("\r\n"));
#endif
  return(0);
}

int8_t rtc_set_alarm(bool hourly)
{
  uint8_t cmd[5];

  cmd[0] = 0x0b;                      /* Alarm 2 address */
  if (hourly)
    cmd[1] = 0x00;                    /* minutes is 0 */
  else
    cmd[1] = 0x80;                    /* every minute */
  cmd[2] = 0x80;                      /* every hour */
  cmd[3] = 0x80;                      /* every day */
  cmd[4]
    = (0 << 7)                        /* enable oscillator */
    | (0 << 6)                        /* disable square wave */
    | (0 << 5)                        /* don't force temp conversion */
    | (1 << 4)                        /* square wave freq */
    | (1 << 3)                        /* square wave freq */
    | (1 << 2)                        /* Output alarm, not sqwv */
    | (1 << 1)                        /* enable alarm 2 interrupt */
    | (0 << 0)                        /* disable alarm 1 interrupt */
    ;

  if (i2c2_transfer7(DS3231_I2C, DS3231_ADDR7BIT, cmd, 5, 0, 0))
    return(RTC_I2C_ERROR);
  return(0);
}

int8_t rtc_alarm(struct rtc * rp)
{
  uint8_t cmd[6];
  cmd[0] = 0x07;                      /* Alarm 1 address */
  cmd[1] = rp->seconds;
  cmd[2] = rp->minutes;
  cmd[3] = rp->hours;
  cmd[4] = rp->day_of_week | (1<<6);  /* use dow not dom */
  dbg(tx_puts("rtc_alarm set alarm... "));
  if (i2c2_transfer7(DS3231_I2C, DS3231_ADDR7BIT, cmd, 5, 0, 0))
    return(RTC_I2C_ERROR);
  dbg(tx_puts("rtc_alarm i2c2_transfer7 done\r\n"));
  cmd[0] = 0x0e;
  cmd[1]
    = (0 << 7)                        /* enable oscillator */
    | (0 << 6)                        /* disable square wave */
    | (0 << 5)                        /* don't force temp conversion */
    | (1 << 4)                        /* square wave freq */
    | (1 << 3)                        /* square wave freq */
    | (1 << 2)                        /* Output alarm, not sqwv */
    | (0 << 1)                        /* disable alarm 2 interrupt */
    | (1 << 0)                        /* enable alarm 1 interrupt */
    ;
  dbg(tx_puts("rtc_alarm enable alarm... "));
  if (i2c2_transfer7(DS3231_I2C, DS3231_ADDR7BIT, cmd, 2, 0, 0))
    return(RTC_I2C_ERROR);
  dbg(tx_puts("rtc_alarm i2c2_transfer7 done\r\n"));
  return(0);
}

/*
  After alarm has gone off, de-assert to allow it to fire again.
 */
int8_t rtc_rearm(void)
{
  uint8_t buf[2];
  buf[0] = DS3231_STATUS;
  buf[1]
    = (0 << 7)                        /* start oscillator */
    | (0 << 6)
    | (0 << 5)
    | (0 << 4)
    | (0 << 3)                        /* enable int output */
    | (0 << 2)                        /* BSY XXX */
    | (0 << 1)                        /* Clear alarm 2 flag */
    | (0 << 0);                       /* Clear alarm 1 flag */
  if (i2c2_transfer7(DS3231_I2C, DS3231_ADDR7BIT, buf, 2, 0, 0))
    return(RTC_I2C_ERROR);
  return(0);
}

extern void rtc_user(struct rtc * rp);

