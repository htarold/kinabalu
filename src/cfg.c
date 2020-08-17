/*
  Copyright 2020 Harold Tay LGPLv3
  Parsing config file contents.
 */

#include <string.h>
#include <ctype.h>
#include "sd2.h"
#include "fil.h"
#include "fmt.h"
#include "tx.h"
#include "rtc.h"
#include "tick.h"
#include "cfg.h"
#include "cfg_parse.h"
#define MHZ 48
#include "delay.h"
#include "attn.h"

char cfg_sitename[6];
char cfg_unit[2];
uint16_t cfg_line_number;

#define CFG_DBG
#ifdef CFG_DBG
#define dbg(x) x
#else
#define dbg(x) /* nothing */
#endif

void cfg_panic(char * s, int16_t d)
{
  attn_init();
  attn_on();
  for ( ; ; ) {
    if (d)
      cfg_log_attr(s, d);
    else
      cfg_log(s, strlen(s));
    if (cfg_line_number)
      cfg_log_attr("at line", cfg_line_number);
    delay_ms(2000);
  }
}

/*
  Find a file with name AAAAA-n LOG.
 */
static er_t match_cfg_file(struct dirent * dp)
{
  char * fn;
  char sitename[sizeof(cfg_sitename)];
  int8_t i;

  fn = (char *)dp;
  if (strncmp(" LOG", fn + 7, 4)) return(1);

  /*
    Site name max 5 chars long, but can be shorter.
   */
  for (i = 0; i < 5; i++) {
    if (!isdigit(fn[i]) &&
        !isupper(fn[i]) &&
        '_' != (fn[i])) return(1);
    sitename[i] = fn[i];
  }
  if (i < 3) return(1);
  sitename[i] = '\0';

  if ('-' != fn[i]) return(1);
  i++;
  if (!isdigit(fn[i])) return(1);
  cfg_unit[0] = fn[i];
  cfg_unit[1] = 0;
  i++;
  if (fn[i] != ' ') return(1);

  dbg(tx_puts("Found a config file:>>"));
  dbg(tx_puts(sitename));
  dbg(tx_puts("<<\r\n"));
  CFG_PANIC((*cfg_sitename), "Multiple config files found!", 0);
  memcpy(cfg_sitename, sitename, sizeof(cfg_sitename));
  return(0);
}

static struct fil cfg;
static uint32_t cfg_offset = 0;

void cfg_unget(char * tok, int8_t len)
{
  CFG_PANIC((len > cfg_offset), "Internal sd_buffer underflow", 0);
  cfg_offset -= len;
  for (len--; len > -1; len--)
    if ('\n' == tok[len]) cfg_line_number--;
}

char cfg_get(void)
{
  char ch;
  int8_t er;
  er = fil_seek(&cfg, cfg_offset);
  CFG_PANIC((er != 0), "fil_seek error ", er);
  ch = sd_buffer[cfg_offset & 511];
  cfg_offset++;
  if ('\n' == ch) cfg_line_number++;
  return(ch);
}

/*
  Returns number of timespecs found
 */
int8_t cfg_init(bool flag_need_sync)
{
  char directive[8];
  int8_t er, i, nr_timespecs;
  bool flag_synced;

  tick_init();
  *cfg_sitename = 0;
  dbg(tx_puts("Calling fil_scan_cwd\r\n"));

  er = fil_scan_cwd(0, match_cfg_file, &cfg);
  CFG_PANIC((er != 0), "fil_scan_cwd:", er);
  CFG_PANIC((*cfg_sitename == '\0'),
    "Can't find filename <site>-<n>.LOG", 0);
  dbg(tx_puts("sitename = \""));
  dbg(tx_puts(cfg_sitename));
  dbg(tx_puts("\"\r\n"));

  /*
    Begin evaluating the config
   */

  cfg_line_number = 1;
  nr_timespecs = 0;
  flag_synced = (flag_need_sync?false:true);

  for ( ; ; ) {
    uint32_t bol;
    char ch;

    for ( ; ; ) {
      if (is_whitespace()) continue;
      ch = cfg_get();
      if ('\n' == ch) continue;
      break;
    }
    cfg_unget(&ch, 1);

    bol = cfg_offset;
    for (i = 0; ; i++) {
      CFG_PANIC((i >= sizeof(directive)-1), "Directive too long", 0);
      directive[i] = cfg_get();
      if (!isalpha(directive[i])) {
        cfg_unget(directive+i, 1);
        break;
      }
    }

    CFG_PANIC((i >= sizeof(directive)-1), "Unknown directive", 0);
    directive[i] = '\0';
    dbg(tx_puts("Found directive:>>"));
    dbg(tx_puts(directive));
    dbg(tx_puts("<<\r\n"));

    is_whitespace();

    if (0 == strcmp(directive, "sync")) {
      struct rtc rtc;
      er = rtc_now(&rtc);
      CFG_PANIC((er != 0), "rtc now error:", er);
      er = is_datetime(&rtc);
      CFG_PANIC((er <= 0), "Error reading date/time for sync", 0);
      er = rtc_init(&rtc);
      CFG_PANIC((er != 0), "Error setting time:", er);
      er = fil_seek(&cfg, bol);
      if (er) return(er);
      sd_buffer[bol & 511] = '#';
      flag_synced = 1;
      continue;
    }

    if (0 == strcmp(directive, "record")) {
      er = is_timespec();
      CFG_PANIC((er <= 0), "Bad time specification, er=", er);
      nr_timespecs++;
      continue;
    }

    if (0 == strcmp(directive, "end")) break;

    cfg_panic("Unknown directive", 0);
  }

  while (!flag_synced) {
    struct rtc now;
    rtc_user(&now);
    if (!rtc_init(&now)) break;
  }

  cfg_log_lit("\r\nStarting");
  return(nr_timespecs);
}

static void append(char * buf, uint16_t len)
{
  uint16_t i;
  fil_append(&cfg, (uint8_t *)buf, len);
  for (i = 0; i < len; i++)
    tx_putc(buf[i]);
}
  
static uint8_t unbcd(uint8_t b) { return(((b>>4)&0xf)*10 + (b&0xf)); }
static void maybe_put_stamp(void)
{
  if (tick_need_stamp()) {
    char * stamp;
    struct rtc now;
    rtc_now(&now);
    append("\r\n", 2);
    stamp = rtc_print(&now);
    append(stamp, strlen(stamp));
    /* While were here, update our dirent with the time */
    cfg.wdate = FIL_DATE(2000+unbcd(now.year),
      unbcd(now.month), unbcd(now.day_of_month));
    cfg.wtime = FIL_TIME(unbcd(now.hours),
      unbcd(now.minutes), unbcd(now.seconds));
  }
}

void cfg_log(char * buf, uint16_t len)
{
  maybe_put_stamp();
  append(" ", 1);
  append(buf, len);
}

void cfg_logs(char * buf) { cfg_log(buf, strlen(buf)); }

void cfg_log_attr(char * attr, int16_t val)
{
  char * cp;
  cfg_log(attr, strlen(attr));
  append("=", 1);
  cp = fmt_i16d(val);
  append(cp, strlen(cp));
}

void cfg_log_lattr(char * attr, int32_t val)
{
  char * cp;
  cfg_log(attr, strlen(attr));
  append("=", 1);
  cp = fmt_i32d(val);
  /* cp = fmt_32x(val); */
  append(cp, strlen(cp));
}
void cfg_log_ulattr(char * attr, uint32_t val)
{
  char * cp;
  cfg_log(attr, strlen(attr));
  append("=", 1);
  cp = fmt_u32d(val);
  /* cp = fmt_32x(val); */
  append(cp, strlen(cp));
}

void cfg_log_sync(void) { fil_save_dirent(&cfg, 0, true); }
