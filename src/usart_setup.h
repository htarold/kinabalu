#ifndef USART_SETUP_H
#define USART_SETUP_H
/*
  Copyright 2020 Harold Tay LGPLv3
  Convenience function to set up the console usart.
 */
#include <libopencm3/stm32/usart.h>

extern char usart_getc(void);
extern void usart_setup(
  uint32_t usart,
  uint32_t port,
  uint32_t txbit,
  uint32_t af, uint32_t bps);

#define TX_RCC       RCC_USART1
#define TX_USART     USART1
#define TX_PORT      GPIOA
#define TX_BIT       GPIO9
#define TX_AF        GPIO_AF1

#define RX_PORT      GPIOA
#define RX_BIT       GPIO10
#define RX_AF        GPIO_AF1

#endif /* USART_SETUP_H */
