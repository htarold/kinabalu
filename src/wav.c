/*
  Copyright 2020 Harold Tay LGPLv3
  Save a wav file of a certain size.
  Assumes 16-bit samples only, and everything is little-endian.
 */

#include <stdlib.h>                    /* for div() */
#include <ctype.h>                     /* for toupper() */
#include <string.h>                    /* for strcpy() */
#include "sd2.h"
#include "fil.h"
#include "rtc.h"
#include "wav.h"
#include "fmt.h"
#include "cfg.h"                      /* for cfg_log_lattr() */

struct wav_header {
  uint32_t chunk_id;                  /* 0x46464952 (LE) */
  uint32_t chunk_size;                /* 5767168 - 8 */
  uint32_t format;                    /* 0x45564157 (LE) */
  uint32_t subchunk1_id;              /* 0x20746d66 (LE) */
  uint32_t subchunk1_size;            /* 16 (LE) */
  uint16_t audio_format;              /* 1 (LE) */
  uint16_t num_channels;              /* 1 or 2 (LE) */
  uint32_t sample_rate;               /* 48000 (LE) */
  uint32_t byte_rate;                 /* 96000 (LE) */
  uint16_t block_align;               /* 2 (LE) */
  uint16_t bits_per_sample;           /* 16 */
  uint32_t subchunk2_id;              /* 0x61746164 (LE) */
  uint32_t subchunk2_size;            /* 5767168 - 44 */
  /* raw data follows */
};

#define WAV_CHUNK_ID        0x46464952
/* #define WAV_CHUNK_SIZE      (5767168 - 8) */
#define WAV_FORMAT          0x45564157
#define WAV_SUBCHUNK1_ID    0x20746d66
#define WAV_SUBCHUNK1_SIZE  16
#define WAV_AUDIO_FORMAT    1
#define WAV_BLOCK_ALIGN     2
#define WAV_BITS_PER_SAMPLE 16
#define WAV_SUBCHUNK2_ID    0x61746164
/* #define WAV_SUBCHUNK2_SIZE  (5767168 - 44) */

static uint32_t wav_start_cluster;
static uint32_t wav_nr_bytes_remaining;
static uint8_t wav_nr_channels;       /* either 1 or 2 only */
static struct fil wav_f;

#ifndef WAV_SPS
#error WAV_SPS not defined
#endif

#define DBG_WAV
#ifdef DBG_WAV
#include "tx.h"
#define dbg(x) x
#else
#define dbg(x) /* nothing */
#endif

static char b36chars[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
/* buf[] has leading zeros */
static void convert_to_base36(uint16_t d, char buf[], int8_t len)
{
  div_t dv;
  if (len <= 0 )return;
  do {
    len--;
    dv = div(d, 36);
    buf[len] = b36chars[dv.rem];
    d = dv.quot;
  } while (len);
}

static uint16_t convert_from_base36(char buf[], uint8_t len)
{
  uint16_t d;
  uint8_t i;

  d = 0;
  for (i = 0; i < len; i++) {
    d *= 36;
    if (buf[i] >= '0' && buf[i] <= '9') {
      d += buf[i] - '0';
    } else if (buf[i] >= 'A' && buf[i] <= 'Z') {
     d += buf[i] - 'A';
    } else
      return(-1);
  }
  return(d);
}

static void itoa2(uint8_t d, char * s)
{
  div_t dv;
  dv = div(d, 10);
  *s++ = '0' + (dv.quot%10);
  *s = '0' + dv.rem;
}

static uint8_t unbcd(uint8_t b) { return(10*((b >> 4)) + (b & 0x0f)); }
static uint8_t tobcd(uint8_t d) { return(((d/10)<<4) | d%10); }
void wav_make_names(struct rtc * rp, char site[5], uint8_t unit,
  char fn[11], char lfn[26])
{
  uint8_t i;
  /*
    create the names from timestamp.  (ym)(dhm)(u)(site)
   */
  unit %= 36;
  convert_to_base36((unbcd(rp->year) * 12) +
    unbcd(rp->month) - 1, fn+0, 2);
  convert_to_base36(((((unbcd(rp->day_of_month)-1)*24) +
    unbcd(rp->hours)) * 60) + unbcd(rp->minutes), fn+2, 3);
  fn[5] = b36chars[unit];
  for (i = 0; i< 5; i++) {
    if (site[i])
      fn[6+i] = toupper(site[i]);
    else
      fn[6+i] = '_';
  }
  fn[11] = '\0';

  if (lfn) {
    for (i = 0; i < 5 && site[i]; i++)
      lfn[i] = site[i];
    lfn += i;
    *lfn++ = '-';
    *lfn++ = b36chars[unit];
    *lfn++ = '-';
    strcpy(lfn, fmt_x(rp->year)); lfn += 2;
    strcpy(lfn, fmt_x(rp->month)); lfn += 2;
    strcpy(lfn, fmt_x(rp->day_of_month)); lfn += 2;
    *lfn++ = '-';
    strcpy(lfn, fmt_x(rp->hours)); lfn += 2;
    strcpy(lfn, fmt_x(rp->minutes)); lfn += 2;
    strcpy(lfn, ".wav");
  }
}


int8_t wav_record(struct rtc * rp, char fn[11], char lfn[27],
  uint16_t seconds, uint8_t nr_channels)
{
  static struct wav_header w;
  int8_t er;
  uint32_t file_bytes;

  if (nr_channels < 1 || nr_channels > 2)
    nr_channels = 1;
  wav_nr_channels = nr_channels;

  file_bytes = seconds * WAV_SPS * 2; /* 2 bytes per sample */
  file_bytes *= wav_nr_channels;

  file_bytes += sizeof(w);
  if (file_bytes & 0x000003ff) {      /* round up to nearest k */
    file_bytes += 1024;
    file_bytes &= ~(0x000003ff);
  }

  er = fil_create(fn, lfn, &wav_f);
  if (er) return(er);
  er = sd_buffer_sync();
  if (er) return(er);

  er = fil_find_free_clusters(file_bytes/1024, &wav_start_cluster);
  if (er) {
    dbg(tx_msg("wav_record:fil_find_free_clusters returned ", er));
    return(er);
  }
  wav_f.file_size = sizeof(w);

  wav_f.head = wav_start_cluster;

  /* more meaningful to use CREATION time */
  if (rp) {
    wav_f.wdate =
      FIL_DATE((unbcd(rp->year)+2000),
      unbcd(rp->month),
      unbcd(rp->day_of_month));
    wav_f.wtime =
      FIL_TIME(unbcd(rp->hours), unbcd(rp->minutes), unbcd(rp->seconds));
  }

  er = fil_save_dirent(&wav_f, 0, true);
  if (er) {
    dbg(tx_msg("wav_record:fil_save_dirent returned ", er));
    return(er);
  }
  er = sd_buffer_sync();
  if (er) {
    dbg(tx_msg("wav_record:sd_buffer_sync returned ", er));
    return(er);
  }

  wav_f.file_size = wav_nr_bytes_remaining = file_bytes;
  cfg_log_lattr("bytes_to_write", file_bytes);

  /*
    From now, no longer using fil API and sd_buffer[] not used.
    The complete file basically exists on disk, but contains rubbish.
   */

  er = sd_bwrites_begin(fil_sector_address(wav_start_cluster),
    file_bytes/512);
  if (er) {
    dbg(tx_msg("wav_record:sd_bwrites_begin returned ", er));
    return(er);
  }

  w.chunk_id = WAV_CHUNK_ID;
  w.chunk_size = file_bytes - 8;
  w.format = WAV_FORMAT;
  w.subchunk1_id = WAV_SUBCHUNK1_ID;
  w.subchunk1_size = WAV_SUBCHUNK1_SIZE;
  w.audio_format = WAV_AUDIO_FORMAT;
  w.num_channels = wav_nr_channels;
  w.sample_rate = WAV_SPS;
  w.byte_rate = 2*WAV_SPS*wav_nr_channels;
  w.block_align = wav_nr_channels * 2;
  w.bits_per_sample = WAV_BITS_PER_SAMPLE;
  w.subchunk2_id = WAV_SUBCHUNK2_ID;
  w.subchunk2_size = file_bytes - 44;

  er = wav_add((void *)&w, sizeof(w)/2);
  if (er)
    dbg(tx_msg("wav_record:wav_add returned ", er));
  return(er);
}

int8_t wav_add(uint16_t * buf, uint16_t word_count)
{
  uint16_t byte_count;
  int8_t er;

  byte_count = word_count * 2;
  if (wav_nr_bytes_remaining < byte_count)
    byte_count = wav_nr_bytes_remaining;

  er = sd_bwrites((void *)buf, byte_count);

  if (er) {
    dbg(tx_msg("wav_add:sd_bwrites returned ", er));
    dbg(tx_msg("wav_add:sd_bwrites byte_count = ", byte_count));
    dbg(tx_puts("bytes left = "));
    dbg(tx_putdec32(wav_nr_bytes_remaining));
    dbg(tx_puts("\r\n"));
    return(er);
  }
  wav_nr_bytes_remaining -= byte_count;

  if (wav_nr_bytes_remaining)
    return(0);

  er = sd_bwrites_end();
  if (er) return(er);
  /*
    File is complete except for file size.
    wav_f.f.file_size has been updated.
   */
  er = fil_save_dirent(&wav_f, 0, true);
  if (er) {
    dbg(tx_msg("wav_add:fil_save_dirent returned ", er));
    return(er);
  }
  dbg(tx_puts("wav_add:completed writing, returning 1\r\n"));
  return(1);
}
