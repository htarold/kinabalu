/*
  Copyright 2020 Harold Tay LGPLv3
  SD card access functions.  Arch specific functions in sd-spi.
 */
#include "sd2.h"

#include "tx.h" /* XXX */
#undef DBG_DEEP
#define DBG_SD
#ifdef DBG_SD

#include "tx.h"
#include "fmt.h"
#define dbg_msg(s, v) do { tx_msg(s, v); } while(0)
#define dbg(a) a

#ifdef DBG_DEEP
#define put(x) do { tx_putc('o'); tx_puthex(x); sd_xfer(x); } while (0)
#define get() ({uint8_t x = sd_xfer(0xff); tx_putc('i'); tx_puthex(x); x; })
#else
#define put(x) (void)sd_xfer(x)
#define get() sd_xfer(0xff)
#endif

#else

#define dbg_msg(s, v) ; /* nothing */
#define dbg(a)  /* nothing */
#define put(x) (void)sd_xfer(x)
#define get() sd_xfer(0xff)

#endif

uint8_t sd_buffer[512];

/*
  For timeout purposes, assuming /2 and 8MHz, 1 byte takes 2us to
  clock out, and 1ms is 500 bytes.  Clock in multiples of 10
  bytes.  If we clock slower than this, timeouts will only be
  longer.
 */
#define SKIP_MS(skip, ms) skip_(skip, ((uint16_t)ms)*90U)
static uint8_t skip_(uint8_t skip, uint16_t x10bytes)
{
  uint8_t i, res;
  do {
    for(i = 0; i < 10; i++) {
      res = sd_xfer(0xff);
      if (skip != res) goto done;
    }
  } while (x10bytes--);
done:
#ifdef DBG_SD
#ifdef DBG_DEEP
  tx_putc('i');
  tx_puthex(res);
#endif
#endif
  return(res);
}

static void putdw(uint32_t dword)
{
  put((dword>>24) & 0xff);
  put((dword>>16) & 0xff);
  put((dword>>8) & 0xff);
  put(dword & 0xff);
}

uint8_t sd_result;

static uint8_t cmd(uint8_t c, uint32_t arg, uint8_t crc)
{
  put(c|(1<<6));
  putdw(arg);
  put(crc);

  /* XXX Should skip anything with the high bit set? */
  sd_result = SKIP_MS(0xff, 50);

  return(sd_result);
}
/* Call after cmd() if needed */
static uint32_t sd_response(void)
{
  uint32_t u;
  u = get();  u <<= 8;
  u |= get(); u <<= 8;
  u |= get(); u <<= 8;
  u |= get();
  return(u);
}

static int8_t appcmd(uint8_t c, uint32_t arg, uint8_t crc)
{
  int8_t i, res;
  res = 0;
  for (i = 40; i > 0; i--, sd_delay(2)) {
    res = cmd(55, 0, 0x65);
    if (0 == res                      /* ready after init */
      || 1 == res) {                  /* idle during init*/
      res = cmd(c, arg, crc);
      if (!res) return(0);
    }
  }
  return(SD_ERR_APPCMD55);
}

static int8_t card_init(void)
{
  uint8_t i, res;

  dbg(tx_puts("Entered card_init()\r\n"));
  sd_buffer_checkin(SD_ADDRESS_NONE);

  (void)SKIP_MS(0xff, 1);

  dbg(tx_puts("Sending CMD0\r\n"));

  for(i = 0; ; i++, sd_delay(i)) {
    if (20 == i) return(SD_TMO_IDLESTATE0);
    if (1 == cmd(0, 0, 0x95)) break;
  }

  (void)SKIP_MS(0xff, 20);

  dbg(tx_puts("Sending CMD8\r\n"));

  for(i = 0; ; i++, sd_delay(i)) {
    if (10 == i) return(SD_TMO_IFCOND8);
    if (1 == cmd(8, 0x1aa, 0x87)) break;
  }
  /* XXX Response if R7 (R1 then 4 bytes) */
  if (sd_response() != 0x1aa) return(SD_ECHO_ERR8);

  dbg(tx_puts("Sending CMD55\r\n"));

  res = appcmd(41, 0x40000000, 0x77);
  if (res < 0) return(res);
  if (res) return(SD_ERR_ACMD);

  dbg(tx_puts("Sending CMD58\r\n"));

  for(i = 0; ; i++, sd_delay(i)) {
    if (10 == i) return(SD_TMO_OCR58);
    res = cmd(58, 0, 0);
    if (1 == res || 0 == res) break;
  }
  /* XXX Response of cmd58 is R3 (R1 then 4 bytes) */

  if (!(sd_response() & (1UL<<22))) return(SD_NOT_SDHC);

  dbg(tx_puts("card_init complete\r\n"));

  return(0);
}

int8_t sd_init(void)
{
  int8_t i, res;

  for (i = 0; i < 10; i++, sd_delay(i)) {
    int8_t j;
    dbg_msg("sd_init attempt ", i);
    sd_spi_init();
    sd_cs_hi(); /* XXX */
    dbg(tx_puts("spi initialised, clocking 80..."));
    for (j = 80; j > 0; j--) {
      (void)sd_xfer(0xff);
    }
    dbg(tx_puts("clocked\r\n"));
    sd_cs_lo();
    dbg(tx_puts("card_init..."));
    res = card_init();
    dbg_msg("init() returned ", res);
    sd_cs_hi();
    if (!res) break;
    dbg_msg("card_init error: ", res);
  }
  sd_go_fast();
  return(res);
}

static int8_t bread(uint32_t addr)
{
  int8_t res;
  uint16_t i;

  res = 0;

  sd_cs_lo();
  put(17|(1<<6));
  putdw(addr);
  put(0);

  sd_result = SKIP_MS(0xff, 300);
  if (0 != sd_result) {
    res = SD_TMO_READBLK;
    goto done;
  }

  sd_result = SKIP_MS(0xff, 500);
  if (0xfe != sd_result) {
    res = SD_TMO_READBLK2;
    goto done;
  }

  for(i = 0; i < sizeof(sd_buffer); i++)  
    sd_buffer[i] = sd_xfer(0xff);

  putdw(0xffffffff);                  /* "CRC bytes" */
done:
  sd_cs_hi();
  if (res) { dbg_msg("bread error: ", res); }
  return(res);
}

int8_t sd_bread(uint32_t addr)
{
  int8_t res;
  if ((res = bread(addr)))
    if (!sd_init())
      res = bread(addr);
  return(res);
}

static int8_t bwrite(uint32_t addr)
{
  int8_t res;
  uint16_t i;

  res = 0;

  sd_cs_lo();
  put(24|(1<<6));
  putdw(addr);

  /*
    Need massive timeout after write_multiple_block?
   */
  for (i = 4; i > 0; i--) {
    sd_result = SKIP_MS(0xff, 700);
    if (sd_result != 0xff) break;
  }

  if (0 != sd_result) {
    dbg_msg("bwrite:SD_TMO_WRITEBLK line ", __LINE__);
    res = SD_TMO_WRITEBLK;
    goto done;
  }

  put(0xfe);                          /* start token */
  for(i = 0; i < sizeof(sd_buffer); i++) {
    (void)sd_xfer(sd_buffer[i]);
  }

  put(0);                             /* "CRC" */
  put(0);

  sd_result = SKIP_MS(0xff, 700);
  if ((sd_result & 0x1f) != 5) {
    dbg_msg("bwrite:SD_ERR_WRITEBLK, sd_result=", sd_result);
    res = SD_ERR_WRITEBLK;
    goto done;
  }

  sd_result = SKIP_MS(0, 700);
  if (!sd_result) {
    dbg_msg("bwrite:SD_TMO_WRITEBLK2 line ", __LINE__);
    res = SD_TMO_WRITEBLK2;
    goto done;
  }

done:
  sd_cs_hi();
  if (res) { dbg_msg("bwrite error: ", res); }
  return(res);
}

int8_t sd_bwrite(uint32_t addr)
{
#ifdef SD_READONLY
#warning SD_READONLY defined
  return(0);
#else
  int8_t res;
  if ((res = bwrite(addr)))
    if (!sd_init())
      res = bwrite(addr);
  return(res);
#endif
}

static int8_t failed(int8_t res) { sd_cs_hi(); return(res); }

static int16_t bwrites_offset;        /* # payload bytes written in current sector */

int8_t sd_bwrites_begin(uint32_t addr, uint32_t nr_sectors)
{
  int8_t res;
#ifndef SIMULATE_MULTI
  sd_cs_lo();
  res = appcmd(23, nr_sectors & 0x04fffff, 0);
  if (res < 0) return(failed(res));
  if (res > 0) return(failed(SD_ERR_APPCMD23)); /* XXX */
  /* if (res&0x01) return(failed(SD_ERR_APPCMD23)); */
  res = cmd(25, addr, 0);
  if (res) return(failed(SD_ERR_WR_MULTI));
#endif
  bwrites_offset = 0;

  return(0);
}

int8_t sd_bwrites(uint8_t * buf, uint16_t len)
{
  int8_t res;

  while (len > 0) {
#ifndef SIMULATE_MULTI
    if (0 == bwrites_offset)          /* start a new data block */
      sd_xfer(0xfc);                  /* start token */
    sd_xfer(*buf++);
#endif
    len--;
    bwrites_offset++;
    if (512 == bwrites_offset) {
      bwrites_offset = 0;
#ifndef SIMULATE_MULTI
      sd_xfer(0xff); sd_xfer(0xff);   /* "crc" */
      res = SKIP_MS(0xff, 4);
      if ((res & 0x1f) != 0x05) {
        int8_t code;
        if (res == 0x0b) return(failed(SD_REJ_DATA_CRC));
        if (res == 0x0d) return(failed(SD_REJ_DATA_WRITE));
        code = (res >> 1) & 0xf;
        return(failed(SD_REJ_DATA - code));
      }
      if (0xff != SKIP_MS(0, 700))    /* skip "busy" */
        if (0xff != SKIP_MS(0, 700))
          return(failed(SD_TMO_WRITEBLK));
#endif
    }
  }
  return(0);
}

int8_t sd_bwrites_end(void)
{
#ifndef SIMULATE_MULTI
  int8_t res;

  if (bwrites_offset) {               /* last sector not completed */
    dbg_msg("sd_bwrites_end partial sector: ", bwrites_offset);
    while (bwrites_offset < 512) {
      sd_xfer(0x00);
      bwrites_offset++;
    }
  }

  sd_xfer(0xfd);                      /* stop tran */
  (void)SKIP_MS(0xff, 700);
  res = SKIP_MS(0x00, 700);
  dbg_msg("sd_bwrites_end:skip returned ", res);

  sd_cs_hi();

  return(res?0:SD_TMO_WRITEBLK2);
#else
  return(0);
#endif
}

/*
  sd_buffer[] can support different users.  sd_buffer_checkout(addr)
  fills the buffer from addr after first saving its current
  contents.  sd_buffer_checkin(addr) relinquishes the buffer, whose
  contents are from addr.
 */
uint32_t current_address = SD_ADDRESS_NONE;
int8_t sd_buffer_sync(void)
{
  return (current_address == SD_ADDRESS_NONE?
          0:
          sd_bwrite(current_address));
}
int8_t sd_buffer_checkout(uint32_t addr)
{
  int8_t res;
  if (addr != current_address) {
    res = sd_buffer_sync();
    if (res) return(res);
    if (addr != SD_ADDRESS_NONE) {
      res = sd_bread(addr);
      if (res) return(res);
    }
    current_address = addr;
  }
  return(0);
}

void sd_buffer_checkin(uint32_t addr)
{
  current_address = addr;
}
