#ifndef CFG_PARSE_H
#define CFG_PARSE_H
/*
  Copyright 2020 Harold Tay LGPLv3
  Parsing time rules.
 */
#include "rtc.h"

extern int8_t is_whitespace(void);

extern int8_t is_timespec(void);

extern int8_t is_datetime(struct rtc * rp);

/*
  Scans all timespec rules, returns the number of minutes it is
  safe to sleep for without missing any scheduled action, and
  *now is updated with the correct alarm values (based on days
  of week, not on days of month).  If 0 is returned, there is no
  time to sleep, activity must occur immediately, and for
  *duration seconds.
 */
extern int16_t cfg_make_alarm(struct rtc * now,
  int16_t * duration, uint8_t * is_mono);

#endif /* CFG_PARSE_H */
