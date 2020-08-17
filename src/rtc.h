#ifndef RTC_H
#define RTC_H
/*
  Copyright 2020 Harold Tay LGPLv3
  Real time clock functions/virtual functions.
 */

#define RTC_I2C_ERROR   -42
#define RTC_NO_OSC      -43
#define RTC_INIT_FAILED -44
#define RTC_ALARM_TMO   -45

#include <stdbool.h>

/*
  All values in BCD.
 */
struct rtc {
  uint8_t seconds;
  uint8_t minutes;
  uint8_t hours;
  uint8_t day_of_month;               /* 1 to 31 */
  uint8_t month;                      /* 1 to 12 */
  uint8_t year;                       /* 0 to 99 */
  uint8_t day_of_week;                /* 1-7, Sunday is 1 */
};

/*
  If _init(0) returns 0, RTC has been initialised with
  date/time, but user can give it new date/time.
  If _init(0) returns NO_OSC, but _now() returns valid
  date/time, it's ok.  After "some time", _init(0) will revert
  to returning 0.
 */
extern int8_t rtc_init(struct rtc * rp);

extern int8_t rtc_i2c_init(
  uint32_t i2c_dev,                   /* peripheral device, e.g.  I2C1 */
  uint32_t gpio_port,                 /* which gpio port */
  uint16_t gpio_sda_scl,              /* gpio ORed bits for sda and scl */
  uint8_t gpio_af);                   /* alt pin function for sda, scl */

extern int8_t rtc_now(struct rtc * rp);
/* Deprecated: */
extern int8_t rtc_set_alarm(bool hourly);
/*
  Only rp->minutes, hours, and day_of_week used.
  Use of wild cards not supported, will be chip specific.
  Therefor must specifiy exact min/hour/dow.
 */
extern int8_t rtc_alarm(struct rtc * rp);
extern int8_t rtc_rearm(void);
extern char * rtc_print(struct rtc * rp);

extern void rtc_user(struct rtc * rp);

#endif /* RTC_H */
