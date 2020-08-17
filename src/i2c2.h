#ifndef I2C2_H
#define I2C2_H
/*
  Copyright 2020 Harold Tay LGPLv3
  Safety wrapper for libopencm3's i2c functions.
  Replaces i2c_transfer7() which will hang if address is not
  answered.
 */

#define I2C2_NACK -46
#define I2C2_ERR  -47
#define I2C2_EBUF -48


extern int8_t i2c2_write(uint32_t i2c,
  uint8_t addr, uint8_t * ary, uint8_t nr);

extern int8_t i2c2_read(uint32_t i2c,
  uint8_t addr, uint8_t * ary, uint8_t nr);

extern int8_t i2c2_transfer7(uint32_t i2c, uint8_t addr,
  uint8_t * wr, uint8_t nr_w, uint8_t * rd, uint8_t nr_r);

#endif /* I2C2_H */
