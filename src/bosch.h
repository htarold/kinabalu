#ifndef BOSCH_H
#define BOSCH_H
/*
  Copyright 2020 Harold Tay LGPLv3
  Read BMP280 (no humidity) and BME280 (with humidity)
 */

#define BOSCH_ADDR_PRI 0x76
#define BOSCH_ADDR_SEC 0x77
#define BOSCH_CHIP_ID_BMP 0x58
#define BOSCH_CHIP_ID_BME 0x60
#define BOSCH_IS_BME(bp) ((bp)->chip_id == BOSCH_CHIP_ID_BME)
#define BOSCH_IS_BMP(bp) ((bp)->chip_id == BOSCH_CHIP_ID_BMP)

#define BOSCH_E_UNKNOWN_CHIP_ID -90
#define BOSCH_E_BUSY            -91

struct bosch {
  int32_t temperature;
  uint32_t pressure;
  int32_t humidity;
  uint8_t address;
  uint8_t chip_id;
  /* Calibration variables, raw buffer: */
  uint16_t T1;
  int16_t  T2, T3;
  uint16_t P1;
  int16_t  P2, P3, P4, P5, P6, P7, P8, P9;
  uint8_t  unused;
  uint8_t  H1;
  int16_t  H2;
  uint8_t  H3;
  /* Next 3 bytes are shared into H4 and H5 */
  uint8_t  E4, E5, E6;
  int8_t   H6;
};

/*
  initialise, load calibration values.
 */
int8_t bosch_init(struct bosch * bp, uint8_t bosch_addr);

/*
  Begin converwion, forced mode.
 */
int8_t bosch_read_start(struct bosch * bp);

/*
  Returns 0 if not busy (ok to read results)
 */
int8_t bosch_is_busy(struct bosch * bp);

/*
  Read results, compensated.
 */
int8_t bosch_read_end(struct bosch * bp);

#endif /* BOSCH_H */
