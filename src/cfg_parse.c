/*
  Copyright 2020 Harold Tay LGPLv3
  Parse config file directives.
 */
#include <stdint.h>
#include <string.h>
#include "cfg_parse.h"
#include "cfg.h"
#include "sd2.h"
#include "cfg.h"
#include "tx.h"
#define MHZ 48
#include "delay.h"

#define CFG_DBG
#ifdef CFG_DBG
#define dbg(x) x
#else
#define dbg(x) /* nothing */
#endif

/*
  Specify date and time to begin recording.
  directive := "record" mnths days hhs:mms duration [mono]
             | "record" wks hhs:mms duration [mono]
  mnths      := mnth_range [,mnth_range]*
  mnth_range := mnth_name [-mnth_name [/num]]
  mnth_name  := "jan" | "feb" | ... 

  days       := day_range[,day_range]*
  day_range  := num[-num[/num]]

  wks        := wk_range[,wk_range]*
  wk_range   := wk_name[-wk_name[/num]]
  wk_name    := "sun" | "mon" | ...

  hhs        := num_range[,num_range]*
  num_range  := num[-num[/num]]
  mms        := num_range[,num_range]*
  mono       := "mono"

  is_*() functions return 1 if true, 0 if false, -1 if error.
  Function may handle error internally (by calling panic() and
  not returning).

  DOW, months, DOMs are all 0-based!
 */

static int8_t is_num(uint16_t * np)
{
  int8_t i;
  uint16_t d;
  char ch;
  d = 0;
  for (i = 0; ; i++) {
    ch = cfg_get();
    if (ch >= '0' && ch <= '9') {
      d *= 10;
      d += (ch - '0');
    } else
      break;
  }
  cfg_unget(&ch, 1);
  *np = d;
  if (i > 0) {
    dbg(tx_msg("is_num found:", d));
    return(1);
  } else
    return(0);
}

static void set_bits(uint64_t * mp,
  uint8_t fr, uint8_t to, uint8_t max)
{
  uint8_t i, j;
  uint16_t step;
  char ch;

  dbg(tx_puts("set_bits: fr,to,max = "));
  dbg(tx_putdec(fr));
  dbg(tx_putc(','));
  dbg(tx_putdec(to));
  dbg(tx_msg(",", max));
  ch = cfg_get();
  if ('/' == ch) {
    int8_t er;
    er = is_num(&step);
    CFG_PANIC((er <= 0), "Expected step value ", er);
    CFG_PANIC((step >= max), "Step value too large:", step);
  } else {
    step = 1;
    cfg_unget(&ch, 1);
  }

  /*
    Because days of month are not a constant number, the case
    where fr > to and step > 1 won't be handled correctly.
   */
  j = 0;
  for (i = fr; ; i++) {
    if (i > max) i = 0;
    if (!j) *mp |= (1ULL<<i);
    j++;
    if (j == step) j = 0;
    if (i == to) break;
  }
}

static int8_t is_num_range(uint64_t * mp, uint8_t max)
{
  int8_t er;
  uint16_t fr, to;
  char ch;

  er = is_num(&fr);
  if (er <= 0) return(er);
  to = fr;
  ch = cfg_get();
  if ('-' == ch) {
    er = is_num(&to);
    if (er <= 0) return(er);
    set_bits(mp, fr, to, max);
  } else {
    *mp |= (1ULL<<fr);
    cfg_unget(&ch, 1);
  }
  return(1);
}

static int8_t is_nums(uint64_t * mp, uint8_t max)
{
  int8_t er;
  char ch;

  *mp = 0ULL;
  er = is_num_range(mp, max);
  if (er <= 0) return(er);
  while (',' == (ch = cfg_get())) {
    er = is_num_range(mp, max);
    CFG_PANIC((er <= 0), "Expected number after comma ", er);
  }
  cfg_unget(&ch, 1);
  return(1);
}

static int8_t is_symbolic(uint8_t * np, char * names)
{
  int i;
  char name[4];
  char * s;

  for (i = 0; i < 3; i++)
    name[i] = cfg_get();
  name[3] = '\0';
  s = strstr(names, name);
  if (s) {
    *np = (s - names)/3;
    dbg(tx_puts("is_symbolic found \""));
    dbg(tx_puts(name));
    dbg(tx_puts("\" in \""));
    dbg(tx_puts(names));
    dbg(tx_msg("\" = ", *np));
    return(1);
  } else {
    dbg(tx_puts("is_symbolic didn't find \""));
    dbg(tx_puts(name));
    dbg(tx_puts("\" in \""));
    dbg(tx_puts(names));
    dbg(tx_puts("\"\r\n"));
  }
  cfg_unget(name, 3);
  return(0);
}

#ifdef CFG_DBG
static void print_bits(uint64_t n)
{
  int8_t i;
  static char digs[] = "0123456789";
  for (i = 0; i < 60; i++) {
    if (n & 1) tx_putc(digs[i%10]);
    else tx_putc('.');
    n >>= 1;
  }
  tx_puts("\r\n");
}
#endif

static int8_t is_symbolic_range(uint64_t * mp, char * names)
{
  int8_t er;
  uint8_t max, fr, to;
  char ch;

  max = (strlen(names)/3) - 1;
  er = is_symbolic(&fr, names);
  if (er <= 0) return(er);
  to = fr;
  ch = cfg_get();
  if ('-' == ch) {
    er = is_symbolic(&to, names);
    dbg(tx_puts("Got end of range\r\n"));
    CFG_PANIC((er <= 0), "Expected another symbolic name ", er);
    set_bits(mp, fr, to, max);
  } else {
    *mp |= (1<<fr);
    cfg_unget(&ch, 1);
  }
  return(1);
}

static int8_t is_symbolics(uint64_t * mp, char * names)
{
  int8_t er;
  char ch;

  *mp = 0ULL;
  er = is_symbolic_range(mp, names);
  if (er <= 0) return(er);
  while (',' == (ch = cfg_get())) {
    er = is_symbolic_range(mp, names);
    if (er <= 0) return(er);
  }
  cfg_unget(&ch, 1);
  return(1);
}

/*
  Also includes comments, i.e. everything after '#' on a line,
  including the newline.
 */
int8_t is_whitespace(void)
{
  int8_t i;
  char ch;
  for (i = 0; ; i++) {
    ch = cfg_get();
    if (' ' == ch) continue;
    if ('\t' == ch) continue;
    if ('\r' == ch) continue;
    if ('#' == ch) {
      do { ch = cfg_get(); } while ('\n' != ch);
      return(1);
    }
    break;
  }
  cfg_unget(&ch, 1);
  return(i > 0);
}

/*
  Unlike rtc, timespec is 0-based.  Conversion will be needed.
 */
struct timespec {
  uint64_t minutes;
  uint32_t hours;
  uint32_t day_of_month;              /* _month or _week is non-0 */
  uint16_t month;
  uint8_t day_of_week;
  uint8_t mono;                       /* 0 => stereo, 1 => mono */
  int16_t duration;                   /* in seconds */
};
#define MAX_RULES 6
static struct timespec timespecs[MAX_RULES];
static uint8_t nr_timespecs;

#ifdef CFG_DBG
static void print_timespec(int8_t i)
{
  if (timespecs[i].mono) tx_puts("    Mono:\r\n");
  else tx_puts("  Stereo:\r\n");
  tx_puts(" Minutes:");
  print_bits(timespecs[i].minutes);
  tx_puts("   Hours:");
  print_bits(timespecs[i].hours);
  tx_puts("   Mdays:");
  print_bits(timespecs[i].day_of_month);
  tx_puts("  Months:");
  print_bits(timespecs[i].month);
  tx_puts("   Wdays:");
  print_bits(timespecs[i].day_of_week);
  tx_msg("duration:", timespecs[i].duration);
}
#endif

int8_t is_timespec(void)
{
  uint64_t map;
  int8_t er;
  char ch;
  uint16_t duration;
#define WEEKDAYNAMES "sunmontuewedthufrisat"
#define MONTHNAMES "janfebmaraprmayjunjulaugsepoctnovdec"

  CFG_PANIC((nr_timespecs >= MAX_RULES),
    "Oops, too many dates/times, > ", MAX_RULES);

  is_whitespace();
  memset(timespecs + nr_timespecs, 0, sizeof(*timespecs));

  /* check if weekday spec or month/day spec */

  if (is_symbolics(&map, WEEKDAYNAMES)) {
    timespecs[nr_timespecs].day_of_week = (uint8_t)map;
  } else if (is_symbolics(&map, MONTHNAMES)) {
    timespecs[nr_timespecs].month = (uint32_t)map;
    is_whitespace();
    er = is_nums(&map, 31);           /* 0-31 inclusive */
    CFG_PANIC((er <= 0), "Can't decipher day of month ", er);
    timespecs[nr_timespecs].day_of_month = (uint32_t)map;
  } else {
    CFG_PANIC(1, "Can't decipher date/time", 0);
  }

  is_whitespace();
  er = is_nums(&map, 23);
  CFG_PANIC((er <= 0), "Can't decipher hours ", er);
  timespecs[nr_timespecs].hours = (uint32_t)map;

  ch = cfg_get();
  CFG_PANIC((ch != ':'), "Missing colon after hours", 0);

  map = 0ULL;
  er = is_nums(&map, 59);
  CFG_PANIC((er <= 0), "Can't decipher minutes ", er);
  timespecs[nr_timespecs].minutes = map;

  is_whitespace();
  er = is_num(&duration);
  CFG_PANIC((er <= 0), "Missing or unparsable duration ", er);
  timespecs[nr_timespecs].duration = (duration<0?-1:duration);

  is_whitespace();

  timespecs[nr_timespecs].mono = 0;    /* default is stereo */
  {
    char token[8];
    int8_t i;
    for (i = 0; i < sizeof(token)-1; i++) {
      token[i] = cfg_get();
      if (token[i] < 'a' || token[i] > 'z') break;
    }
    cfg_unget(token+i, 1);
    if (i > 0) {
      if (0 == strncmp(token, "mono", 4))
        timespecs[nr_timespecs].mono = 1;
      else if (0 == strncmp(token, "stereo", 6))
        timespecs[nr_timespecs].mono = 0;
      else
        CFG_PANIC(1, "not mono, not stereo option", 0);
    }
  }
  /*
    End of line seen.
   */
  CFG_PANIC(('\n' != cfg_get()), "Garbage at end of line", 0);

  dbg(print_timespec(nr_timespecs));

  nr_timespecs++;
  return(1);
}

/*
  Returns a "safe" number of minutes to sleep for, going by this
  rule (never exceeds 60 minutes).  "Safe" means a recording
  scheduled by this rule will not be skipped.
  struct rtc * now has been converted from BCD.
 */
static int16_t safe_sleep_mins(struct rtc * now, struct timespec * tp)
{
  int8_t i;
  if (tp->day_of_week) {              /* rule based on days of week */
    if (!(tp->day_of_week & (1<<now->day_of_week))) {
      dbg(tx_puts("safe_sleep_mins:DOW doesn't match\r\n"));
      goto max_sleep;
    }
  } else {                            /* rule based on days of month */
    if (!(tp->month & (1<<now->month))) goto max_sleep;
    if (!(tp->day_of_month & (1<<now->day_of_month))) goto max_sleep;
  }
  /* Day matches, check hour */
  if (!(tp->hours & (1UL<<now->hours))) {
    dbg(tx_puts("safe_sleep_mins:Hour doesn't match\r\n"));
    goto max_sleep;
  }
  /* Hour matches, find the min. number of minutes to next firing */
  for (i = now->minutes; i <= 59; i++)
    if (tp->minutes & (1ULL<<i)) {
      dbg(tx_msg("safe_sleep_mins:minutes matched at ", i));
      break;
    }
  if (i >= 60) {                      /* nothing scheduled this hour */
    dbg(tx_puts("safe_sleep_mins:minutes don't match\r\n"));
    goto max_sleep;
  }

  return(i - now->minutes);           /* could be 0 */
max_sleep:
  dbg(tx_puts("safe_sleep_mins:sleep to hour\r\n"));
  return(60 - now->minutes);
}

static uint8_t unbcd(uint8_t b) { return(((b>>4)&0xf)*10 + (b&0xf)); }
static uint8_t tobcd(uint8_t n) { return((n%10) | ((n/10)<<4)); }

static void unbcd_some(struct rtc * rp)
{
  rp->seconds = unbcd(rp->seconds);
  rp->minutes = unbcd(rp->minutes);
  rp->hours = unbcd(rp->hours);
  rp->day_of_week = unbcd(rp->day_of_week) - 1;
}
static void tobcd_some(struct rtc * rp)
{
  rp->seconds = tobcd(rp->seconds);
  rp->minutes = tobcd(rp->minutes);
  rp->hours = tobcd(rp->hours);
  rp->day_of_week = tobcd(rp->day_of_week + 1);
}

/*
  On success, *now is set to the correct alarm date/time (only
  minutes, hours, and day_of_week are used, no other fields).
  Return value is the number of minutes of sleep.  If 0, recording
  must be performed immediately, for *duration seconds.
 */
int16_t cfg_make_alarm(struct rtc * now,
  int16_t * duration, uint8_t * is_mono)
{
  int8_t i, selected;
  int16_t mins, min_mins;

  unbcd_some(now);

  min_mins = INT16_MAX;
  selected = nr_timespecs;
  for (i = 0; i < nr_timespecs; i++) {
    mins = safe_sleep_mins(now, timespecs+i);
    if (mins < min_mins) {
      min_mins = mins;
      selected = i;
    }
  }

  dbg(tx_msg("cfg_make_alarm selected rule ", selected));
  dbg(tx_msg("with minutes to wait = ", min_mins));

  do {
    /* min_mins <= 60 */
    now->minutes += min_mins;
    if (now->minutes < 60) break;
    now->minutes -= 60;
    now->hours++;
    if (now->hours < 24) break;
    now->hours = 0;
    now->day_of_week++;
    if (now->day_of_week < 7) break;
    now->day_of_week = 0;
  } while (0);

  tobcd_some(now);
  now->seconds =
  now->day_of_month =
  now->month =
  now->year = 0;
  *duration = timespecs[selected].duration;
  *is_mono = timespecs[selected].mono;
  return(min_mins);                   /* could be 0 */
}

/*
  If returns true, *rp contains the date/time parsed.
  [year month mday] hour:minute[:second] [wday]
  *rp will have been pre-populated with the current time.
  NOTE: values are in BCD.
 */
int8_t is_datetime(struct rtc * rp)
{
  int8_t er;
  uint16_t n;
  uint8_t month, wk;
  char ch;

  is_whitespace();
  er = is_num(&n);
  CFG_PANIC((er <= 0), "Expected numeric value ", er);
  is_whitespace();

  er = is_symbolic(&month, MONTHNAMES);
  if (er > 0) {
    is_whitespace();
    dbg(tx_msg("month found, = ", month));
    if (n > 2000) n -= 2000;
    CFG_PANIC((n > 99), "Year value too large, ", n);
    rp->year = tobcd(n);
    er = is_num(&n);
    is_whitespace();
    CFG_PANIC((er <= 0), "Expected day of month ", er);
    CFG_PANIC((n < 1 || n > 31), "Day value out of range, ", n);
    rp->month = tobcd(month+1);       /* 0-based to 1-based */
    rp->day_of_month = tobcd(n);
    er = is_num(&n);
    is_whitespace();
    CFG_PANIC((er <= 0), "Expected numeric value for hour ", er);
  } else {
    dbg(tx_puts("missing year and month\r\n"));
  }

  CFG_PANIC((n > 23), "Hour out of range, ", n);
  rp->hours = tobcd(n);
  ch = cfg_get();
  CFG_PANIC((ch != ':'), "Expected ':' after hours", 0);
  er = is_num(&n);
  CFG_PANIC((n > 59), "Minutes out of range, ", n);
  rp->minutes = tobcd(n);
  ch = cfg_get();
  if (':' == ch) {
    er = is_num(&n);
    CFG_PANIC((er <= 0), "Expected value for seconds ", er);
    rp->seconds = tobcd(n);
  } else {
    cfg_unget(&ch, 1);
    rp->seconds = 0x30;
  }

  is_whitespace();

  er = is_symbolic(&wk, WEEKDAYNAMES);
  is_whitespace();
  if (er > 0)
    rp->day_of_week = wk + 1;         /* 0-based to 1-based */

  ch = cfg_get();
  CFG_PANIC((ch != '\n'), "Garbage at end of line", 0);

  return(1);
}
