#ifndef VDDA_H
#define VDDA_H
/*
  Copyright 2020 Harold Tay LGPLv3
  Read mocrocontroller's VDD based on internal voltage ref.
 */

extern void vdda_init(void);

extern int16_t vdda_read_mv(void);

#endif /* VDDA_H */
