/*
  Copyright 2020 Harold Tay LGPLv3
  Convenience usart setup function.
 */

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/usart.h>
#include <libopencm3/stm32/gpio.h>
#include "usart_setup.h"

static uint32_t which_usart;
char usart_getc(void)
{
  char ch;
  ch = usart_recv_blocking(which_usart);
  return(ch);
}

void usart_setup(uint32_t usart, uint32_t port, uint32_t txbit, uint32_t af, uint32_t bps)
{
  uint32_t c;

  which_usart = usart;
  c = 0;
#ifdef USART1
  if (usart == USART1) c = RCC_USART1;
#endif
#ifdef USART2
  if (usart == USART2) c = RCC_USART2;
#endif
#ifdef USART3
  if (usart == USART3) c = RCC_USART3;
#endif
#ifdef USART4
  if (usart == USART4) c = RCC_USART4;
#endif
#ifdef USART5
  if (usart == USART5) c = RCC_USART5;
#endif
#ifdef USART6
  if (usart == USART6) c = RCC_USART6;
#endif
  while (!c);
  rcc_periph_clock_enable(c); 
  c = 0;
#ifdef GPIOA
  if (port == GPIOA) c = RCC_GPIOA;
#endif
#ifdef GPIOB
  if (port == GPIOB) c = RCC_GPIOB;
#endif
#ifdef GPIOC
  if (port == GPIOC) c = RCC_GPIOC;
#endif
#ifdef GPIOD
  if (port == GPIOD) c = RCC_GPIOD;
#endif
#ifdef GPIOF
  if (port == GPIOF) c = RCC_GPIOF;
#endif
  while (!c);
  rcc_periph_clock_enable(c);

  gpio_mode_setup(port, GPIO_MODE_AF, GPIO_PUPD_NONE, txbit);
  gpio_set_af(port, af, txbit);
  gpio_mode_setup(port, GPIO_MODE_AF, GPIO_PUPD_NONE, txbit<<1);
  gpio_set_af(port, af, txbit<<1);
  usart_set_baudrate(usart, bps);
  usart_set_databits(usart, 8);
  usart_set_parity(usart, USART_PARITY_NONE);
  usart_set_stopbits(usart, USART_CR2_STOPBITS_1);
  usart_set_mode(usart, USART_MODE_TX_RX);
  usart_set_flow_control(usart, USART_FLOWCONTROL_NONE);
  usart_enable(usart);
}
