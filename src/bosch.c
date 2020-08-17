/*
  Copyright 2020 Harold Tay LGPLv3
  Access functions to BM{E,P}280 environmental sensor.
 */
#include <stdint.h>
#include <string.h>
#include <libopencm3/stm32/i2c.h>
#include "i2c2.h"
#include "bosch.h"

#undef DBG_BOSCH
#ifdef DBG_BOSCH
#include "tx.h"
#include "fmt.h"
#define dbg(x) x
#else
#define dbg(x)
#endif

static int16_t H4(struct bosch * bp)
{
  int16_t h4;
  h4 = (int16_t)bp->E4;
  h4 <<= 4;
  h4 |= (bp->E5 & 0xf);
  return(h4);
}

static int16_t H5(struct bosch * bp)
{
  int16_t h5;
  h5 = (int16_t)(bp->E5 & 0xf0);
  h5 <<= 4;
  h5 |= ((uint16_t)bp->E6);
  return(h5);
}

#ifdef DBG_BOSCH
static void dump_struct(struct bosch * bp)
{
  tx_puts("\r\ntemperature="); tx_puts(fmt_i32d(bp->temperature));
  tx_puts("\r\npressure   ="); tx_puts(fmt_u32d(bp->pressure));
  tx_puts("\r\nhumidity   ="); tx_puts(fmt_i32d(bp->humidity));
  tx_puts("\r\naddress=0x"); tx_puthex(bp->address);
  tx_puts(", chip_id=0x"); tx_puthex(bp->chip_id);
  tx_puts("\r\nT1="); tx_puts(fmt_u16d(bp->T1));
  tx_puts(", T2="); tx_puts(fmt_i16d(bp->T2));
  tx_puts(", T3="); tx_puts(fmt_i16d(bp->T3));
  tx_puts("\r\nP1="); tx_puts(fmt_u16d(bp->P1));
  tx_puts(", P2="); tx_puts(fmt_i16d(bp->P2));
  tx_puts(", P3="); tx_puts(fmt_i16d(bp->P3));
  tx_puts(", P4="); tx_puts(fmt_i16d(bp->P4));
  tx_puts(", P5="); tx_puts(fmt_i16d(bp->P5));
  tx_puts(", P6="); tx_puts(fmt_i16d(bp->P6));
  tx_puts(", P7="); tx_puts(fmt_i16d(bp->P7));
  tx_puts(", P8="); tx_puts(fmt_i16d(bp->P8));
  tx_puts(", P9="); tx_puts(fmt_i16d(bp->P9));
  tx_puts("\r\nH1="); tx_putdec(bp->H1);
  tx_puts(", H2="); tx_putdec(bp->H2);
  tx_puts(", H3="); tx_putdec(bp->H3);
  tx_puts(", E4=0x"); tx_puthex(bp->E4);
  tx_puts(", E5=0x"); tx_puthex(bp->E5);
  tx_puts(", E6=0x"); tx_puthex(bp->E6);
  tx_puts(", H6="); tx_puts(fmt_i16d(bp->H6));
  tx_puts("\r\n");
}
#else
#define dump_struct(x) /* nothing */
#endif

#define PARAMS(bp) I2C2, bp->address

int8_t bosch_init(struct bosch * bp, uint8_t bosch_addr)
{
  int8_t er;
  uint8_t count, reg;

  memset(bp, 0, sizeof(*bp));
  bp->address = bosch_addr;

  /* Get chip ID first */
  reg = 0xd0;
  er = i2c2_transfer7(PARAMS(bp), &reg, 1, &(bp->chip_id), 1);
  dbg(tx_msg("i2c_write returned ", er));
  if (er) return(er);

  if (BOSCH_IS_BMP(bp)) {
    dbg(tx_puts("is BMP\r\n"));
  } else if (BOSCH_IS_BME(bp)) {
    dbg(tx_puts("is BME\r\n"));
  } else
    return(BOSCH_E_UNKNOWN_CHIP_ID);

  /*
    Copy common calib values (and H1 which may be empty).
   */
  if (BOSCH_IS_BMP(bp))
    count = (0x97 - 0x88)+1;
  else
    count = (0xa1 - 0x88)+1;
  reg = 0x88;
  er = i2c2_transfer7(PARAMS(bp), &reg, 1, (void *)&(bp->T1), count);
  dbg(tx_msg("i2c_transfer7 returned ", er));
  if (er) return(er);

  if (BOSCH_IS_BMP(bp)) {
    return(0);
  }
  
  /*
    Copy remaining humidity calib values.
   */
  count = (0xe7 - 0xe1)+1;
  reg = 0xe1;
  er = i2c2_transfer7(PARAMS(bp), &reg, 1, (void *)&(bp->H2), count);
  dbg(tx_msg("i2c_transfer7 returned ", er));
  if (er) return(er);


  return(0);
}

/*
  Set up registers, begin conversion.
 */
int8_t bosch_read_start(struct bosch * bp)
{
  int8_t er;
  uint8_t regs[3];

  if (BOSCH_IS_BME(bp)) {
    /* set humidity oversampling */
    regs[0] = 0xf2;
    regs[1] = 0x5;                    /* oversample 16x */
    er = i2c2_write(PARAMS(bp), regs, 2);
    dbg(tx_msg("BME humidity oversample i2c2_write returned ", er));
    if (er) return(er);
  }

  regs[0] = 0xf4;
  regs[1]                             /* ctrl_meas register */
          = 0x5 << 5                  /* osrs_t, x16 (20-bit resolution) */
          | 0x5 << 2                  /* osrs_p, x16 */
	  | 0x1;                      /* forced mode */
  regs[2]                             /* config register */
          = 0x0 << 5                  /* Tstandby unused in forced */
	  | 0x0 << 2                  /* IIR filter unused in forced */
	  | 0x0;                      /* SPI unused */
  er = i2c2_write(PARAMS(bp), regs, 3);
  dbg(tx_msg("BME ctrl/meas i2c2_write returned ", er));
  if (er) return(er);

  return(0);
}

static void compensate(struct bosch * bp);

int8_t bosch_read_end(struct bosch * bp)
{
  int8_t er;
  uint8_t reg, count, ary[8];
  uint32_t work;

  reg = 0xf7;
  count = (BOSCH_IS_BME(bp)?8:6);
  er = i2c2_transfer7(PARAMS(bp), &reg, 1, ary, count);
  dbg(tx_msg("read_end i2c2_transfer7 returned ", er));
  if (er) return(er);

  bp->pressure
        = ary[0]  /* pressure MSB */ << 24
        | ary[1]  /* pressure LSB */ << 16
	| ary[2]  /* pressure XLSB */ << 8;
  bp->pressure >>= 12;                /* 20 bits */

  work  = ary[3] << 24
        | ary[4] << 16
	| ary[5] << 8;
  work >>= 12;
  bp->temperature = (int32_t)work;

  if (BOSCH_IS_BME(bp)) {
    bp->humidity = (int32_t)ary[6];   /* sign extend? XXX */
    bp->humidity <<= 8;
    bp->humidity |= ary[7];
  } else
    bp->humidity = 0;

  dump_struct(bp);

  compensate(bp);

  dump_struct(bp);

  return(0);
}

#define S32 int32_t
#define U32 uint32_t

static void compensate(struct bosch * bp)
{
  int32_t var1, var2;
  int32_t t_fine;

  /* compensate temperature first */
  var1 = ((((bp->temperature>>3) - ((S32)bp->T1<<1))) * ((S32)bp->T2)) >>11;
  var2 = (((((bp->temperature>>4) - ((S32)bp->T1))
       *    ((bp->temperature>>4) - ((S32)bp->T1))) >> 12)
       * ((S32)bp->T3)) >> 14;
  t_fine = var1 + var2;
  bp->temperature = (t_fine * 5 + 128) >> 8;

  /* pressure compensation page 50 */
  var1 = (((S32)t_fine)>>1) - (S32)64000;
  var2 = (((var1>>2) * (var1>>2)) >> 11)
       * ((S32)bp->P6);
  var2 = var2 + ((var1*((S32)bp->P5))<<1);
  var2 = (var2>>2) + (((S32)bp->P4)<<16);
  var1 = (((bp->P3 * (((var1>>2) * (var1>>2)) >> 13)) >> 3)
       + ((((S32)bp->P2) * var1) >> 1)) >> 18;
  var1 = ((((32768L + var1))*((S32)bp->P1)) >> 15);

  if (var1 == 0) {
    bp->pressure = 0UL;
    goto do_humidity;
  }

  bp->pressure
       = (((U32)(((S32)1048576L) - bp->pressure)
       - (var2>>12)))*3125;
  if (bp->pressure < 0x80000000)
    bp->pressure = (bp->pressure<<1)/((U32)var1);
  else
    bp->pressure = (bp->pressure/(U32)var1) * 2;
  
  var1 = (((S32)bp->P9)
       * ((S32)(((bp->pressure>>3) * (bp->pressure>>3)) >> 13))) >> 12;
  var2 = (((S32)(bp->pressure>>2)) * ((S32)bp->P8)) >> 13;
  bp->pressure
       = (U32)((S32)bp->pressure
       + ((var1 + var2 + bp->P7) >> 4));

do_humidity:

  if (BOSCH_IS_BMP(bp)) {
    bp->humidity = 0;
    return;
  }

  /* Humidity compensation.  Reuse var1 for "v_x1_u32_r" Page 23 */
  var1 = (t_fine - ((S32)76800));
  var1 = (((((bp->humidity << 14) - (((S32)H4(bp)) << 20)
       - (((S32)H5(bp)) * var1)) + ((S32)16384L)) >> 15)
       * (((((((var1 * ((S32)bp->H6)) >> 10) * (((var1
       * ((S32)bp->H3)) >> 11) + ((S32)32768))) >> 10) + ((S32)2097152L))
       * ((S32)bp->H2) + 8192) >> 14));
  var1 = (var1 - (((((var1 >> 15) * (var1 >> 15)) >> 7)
       * ((S32)bp->H1)) >> 4));
  var1 = (var1 < 0?0:var1);
  var1 = (var1 > 419430400?419430400:var1);
  bp->humidity = (U32)var1 >> 12;
}

int8_t bosch_is_busy(struct bosch * bp)
{
  int8_t er;
  uint8_t reg, busy;

  reg = 0xf3;                         /* status register */
  er = i2c2_transfer7(PARAMS(bp), &reg, 1, &busy, 1);
  if (er) return(er);
  busy &= ((1<<4) | (1));
  dbg(tx_msg("bosch_is_busy:status_reg=", busy));
  return(busy?BOSCH_E_BUSY:0);
}
