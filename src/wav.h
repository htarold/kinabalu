#ifndef WAV_H
#define WAV_H
/*
  Copyright 2020 Harold Tay LGPLv3
  Definition of wav file header
  Assumes host is little endian.
 */

#include <stdbool.h>
#include "rtc.h"

/*
  Must be defined according to the ADC sample rate.  This is
  determined by the hardware oscillator onboard.
 */
#ifndef WAV_SPS
#warning WAV_SPS defaults to 48000
#define WAV_SPS 48000
#endif

/*
  Make file names fn[] and lfn[] from time etc.
 */
extern void wav_make_names(struct rtc * rp, char site[5],
  uint8_t unit, char fn[11], char lfn[26]);

/*
  Obtain the timeinfo from a file name.
 */
extern void wav_extract_time(struct rtc * rp, char fn[11]);

/*
  If rp given, is used for file's time stamp.  lfn[] may be 0,
  fn[] must be valid.
 */
extern int8_t wav_record(struct rtc * rp, char fn[11], char lfn[27],
  uint16_t duration_seconds, uint8_t nr_channels);

/*
  Low word of buf[n] is left channel, high word is right
  channel.  If mono, then right channel (high word) is ignored.
  Returns 1 when file is complete, 0 if not yet complete, < 0 on
  error.
  XXX Only stereo files allowed for now.
 */
extern int8_t wav_add(uint16_t * buf, uint16_t count);

#endif /* WAV_H */
