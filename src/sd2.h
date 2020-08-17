#ifndef SD_H
#define SD_H
#include <stdint.h>
/*
  Copyright 2020 Harold Tay LGPLv3
  SD card functions.
  These are device specific and must be provided externally
 */
extern int8_t sd_spi_init(void);      /* return non-0 on error */
extern void sd_spi_reset(void);       /* prior to powering off */
extern void sd_go_fast(void);         /* speed up after sd_spi_init() */
extern void sd_cs_lo(void);
extern void sd_cs_hi(void);
extern uint8_t sd_xfer(uint8_t x);    /* transfer over spi */
/*
  Delay for approx. i*0.25ms.  This is only used during init, so
  busy looping is ok.
 */
extern void sd_delay(uint8_t i);

extern uint8_t sd_result;             /* contains last result... */
#define SD_TMO_IDLESTATE0 -2
#define SD_TMO_IFCOND8    -3
#define SD_ECHO_ERR8      -4
#define SD_ERR_APPCMD55   -5
#define SD_TMO_OCR58      -6
#define SD_NOT_SDHC       -7
#define SD_TMO_READBLK    -8
#define SD_TMO_READBLK2   -9
#define SD_TMO_WRITEBLK   -10
#define SD_TMO_WRITEBLK2  -11
#define SD_ERR_WRITEBLK   -12
#define SD_ERR_APPCMD23   -13
#define SD_ERR_WR_MULTI   -14
#define SD_ERR_ACMD       -15
#define SD_REJ_DATA_CRC   -16
#define SD_REJ_DATA_WRITE -17
#define SD_REJ_DATA       -18
/*
  Values from SD_REJ_DATA to SD_REJ_DATA + 15 indicate cause of
  rejection.  Don't use these values.
 */
extern int8_t sd_init(void);
extern uint8_t sd_buffer[512];        /* all I/O to/from this */
extern int8_t sd_bread(uint32_t addr);
extern int8_t sd_bwrite(uint32_t addr);
extern int8_t sd_bwrites_begin(uint32_t addr, uint32_t nr_sectors);
extern int8_t sd_bwrites(uint8_t * buf, uint16_t len);
extern int8_t sd_bwrites_end(void);

#define SD_ADDRESS_NONE (uint32_t)-1
/*
  Check out the data previously checked in under the same
  address.
 */
extern int8_t sd_buffer_checkout(uint32_t addr);
/*
  Give the current buffer a new address.
 */
extern void sd_buffer_checkin(uint32_t addr);

/*
  Sync the current sector to disk.
 */
extern int8_t sd_buffer_sync(void);

#endif /* SD_H */
