#ifndef TICK_H
#define TICK_H
/*
  Copyright 2020 Harold Tay LGPLv3
  Used by logging, to determine if a new timestamp is needed.
  We only have 1-second resolution on time stamps.
 */

extern void tick_init(void);
extern bool tick_need_stamp(void);
#endif /* TICK_H */
