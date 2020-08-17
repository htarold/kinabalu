#ifndef POWER_H
#define POWER_H
/*
  Copyright 2020 Harold Tay LGPLv3
  Powers on/off all other parts of the board (apart from the MCU
  proper).  This means the SD card, preamp, and I2C bus.  The
  ADC has to have the mode (master or slave) set before powering on.
 */

#define power_setup() /* nothing.  For legacy compatibility. */
#define POWER_MODE_MASTER 1
#define POWER_MODE_SLAVE  0
void power_on(uint8_t mode);
void power_off(void);

#endif /* POWER_H */
