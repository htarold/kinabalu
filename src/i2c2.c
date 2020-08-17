/*
  Copyright 2020 Harold Tay LGPLv3
  Safety wrappers for libopencm3 I2C functions.
 */
#include <libopencm3/stm32/i2c.h>
#include "i2c2.h"

#undef DBG_I2C2
#ifdef DBG_I2C2
#include "tx.h"
#define dbg(x) x
#else
#define dbg(x)
#endif

static int8_t nack(uint32_t i2c)
{
  I2C_ICR(i2c) |= I2C_ICR_NACKCF;
  return(I2C2_NACK);
}

/*
  After fig 231 (212?) in manual.
 */
int8_t i2c2_write(uint32_t i2c, uint8_t addr, uint8_t * ary, uint8_t nr)
{
  i2c_set_7bit_address(i2c, addr);
  i2c_set_write_transfer_dir(i2c);
  i2c_set_bytes_to_transfer(i2c, nr);
  i2c_enable_autoend(i2c);
  dbg(tx_puts("send_start\r\n"));
  i2c_send_start(i2c);
  while (nr > 0) {
    dbg(tx_puts("check nack\r\n"));
    if (i2c_nack(i2c)) return(nack(i2c));
    dbg(tx_puts("int_status\r\n"));
    if (i2c_transmit_int_status(i2c)) {
      dbg(tx_puts("send_data\r\n"));
      i2c_send_data(i2c, *ary++);
      nr--;
    }
  }
  return(0);
}

/*
  After fig 234 in manual.
 */
int8_t i2c2_read(uint32_t i2c, uint8_t addr, uint8_t * ary, uint8_t nr)
{
  uint8_t i;

  i2c_set_7bit_address(i2c, addr);
  i2c_set_read_transfer_dir(i2c);
  i2c_set_bytes_to_transfer(i2c, nr);
  i2c_enable_autoend(i2c);
  i2c_send_start(i2c);

  for (i = 0; i < nr; i++) {
    if (i2c_nack(i2c)) return(nack(i2c));
    while (!(i2c_received_data(i2c))) ;
    ary[i] = i2c_get_data(i2c);
  }
  return(0);
}

int8_t i2c2_transfer7(uint32_t i2c, uint8_t addr,
  uint8_t * wr, uint8_t nr_w, uint8_t * rd, uint8_t nr_r)
{
  int8_t er;

  er = 0;
  dbg(tx_puts("i2c2_transfer7 calling _write\r\n"));
  if (nr_w > 0) {
    er = i2c2_write(i2c, addr, wr, nr_w);
    if (er) goto done;
  }
  dbg(tx_puts("i2c2_transfer7 calling _read\r\n"));
  if (nr_r > 0) {
    er = i2c2_read(i2c, addr, rd, nr_r);
    if (er) goto done;
  }
done:
  return(er);
}
