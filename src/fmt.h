#ifndef FMT_H
#define FMT_H
/*
  Copyright 2020 Harold Tay LGPLv3
  Formatted output.
 */
extern char * fmt_u32d(uint32_t d);
extern char * fmt_i32d(int32_t d);
extern char * fmt_u16d(uint16_t d);
extern char * fmt_i16d(int16_t d);
extern char * fmt_x(uint8_t d);
extern char * fmt_32x(uint32_t x);
#endif /* FMT_H */
