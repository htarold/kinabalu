#ifndef CFG_H
#define CFG_H
#include <stdbool.h>
/*
  Copyright 2020 Harold Tay LGPLv3
  Config file reading and logging.
 */

extern char cfg_sitename[6];
extern char cfg_unit[2];
extern uint16_t cfg_line_number;

extern void cfg_panic(char * s, int16_t d);
#define CFG_PANIC(cond, s, d) \
do { if (cond) { cfg_panic(s, d); } } while (0)

/*
  Returns number of timespecs found.  0 is not considered an
  error at this point.
  cfg_init() may not return if there is a fatal error.
  The usart is assumed to be initialised and useable.
 */
extern int8_t cfg_init(bool flag_need_sync);
extern void cfg_unget(char * tok, int8_t len);
extern char cfg_get(void);
extern void cfg_log(char * buf, uint16_t len);
extern void cfg_logs(char * buf);
#define cfg_log_lit(lit) cfg_log(lit, sizeof(lit)-1)
extern void cfg_log_attr(char * attr, int16_t val);
extern void cfg_log_lattr(char * attr, int32_t val);
extern void cfg_log_ulattr(char * attr, uint32_t val);
/* flush to disk */
extern void cfg_log_sync(void);
#endif /* CFG_H */
