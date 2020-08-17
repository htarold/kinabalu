#ifndef PCM1808_H
#define PCM1808_H
#include <libopencm3/stm32/dma.h>
/*
  Copyright 2020 Harold Tay LGPLv3
  Data are added by DMA to pcm1808_buf[PCM1808_HEAD] (circular
  buffer) starting with PCM1808_HEAD = 0.  Read only behind it,
  not in front.  Values at odd indices are left channel, even
  are right.

  48ksps in stereo means 192000 bytes per second or 5.2us per
  byte.  2048 bytes => 10.6ms which should be plenty.
 */
#define PCM1808_BUFSZ 2560
extern uint16_t pcm1808_buf[PCM1808_BUFSZ];
#define PCM1808_HEAD (PCM1808_BUFSZ-DMA_CNDTR(I2S_DMA, I2S_CHANNEL))

/*
  Errors that might be returned
 */
#define PCM1808_SR_BUSY  -51
#define PCM1808_WS2_HIGH -52
#define PCM1808_WS2_LOW  -53

extern int8_t pcm1808_start(void);    /* also does set up */
extern void pcm1808_stop(void);

/*
  These used only in pcm1808.c, valid for stm32f051c8.
 */
#define I2S_SPI      SPI1
#define I2S_RCC      RCC_SPI1
#define I2S_DMA      DMA1
#define I2S_DMA_RCC  RCC_DMA1
#define I2S_CHANNEL  DMA_CHANNEL2

/* I2S word select pin (left/right channel) */
#define I2S_WS_PORT  GPIOA
#define I2S_WS_BIT   GPIO15
#define I2S_WS_AF    GPIO_AF0
#define I2S_WS_RCC   RCC_GPIOA

/* I2S bit clock pin */
#define I2S_CK_PORT  GPIOB
#define I2S_CK_BIT   GPIO3
#define I2S_CK_AF    GPIO_AF0
#define I2S_CK_RCC   RCC_GPIOB

/* I2S serial data pin */
#define I2S_SD_PORT  GPIOB
#define I2S_SD_BIT   GPIO5
#define I2S_SD_AF    GPIO_AF0
#define I2S_SD_RCC   RCC_GPIOB

/* Tied to WS in hardware so we can read it */
#define I2S_WS2_PORT GPIOB
#define I2S_WS2_BIT  GPIO4
#define I2S_WS2_RCC  RCC_GPIOB

#endif
