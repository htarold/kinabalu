#ifndef DELAY_H
#define DELAY_H
/*
  Copyright 2020 Harold Tay LGPLv3
  Not very accurate timing delays, but convenient.
 */
#ifndef MHZ
#define MHZ 48
#warning MHZ defaults to 48
#endif

#define delay_us(us) \
{ uint32_t i; for (i = 1.33*MHZ*us/8; i > 0; i--) { __asm__("nop"); } }

#define delay_ms(ms) \
{ uint32_t i; for (i = ms; i > 0; i--) { delay_us(1000); } }

#endif /* DELAY_H */
