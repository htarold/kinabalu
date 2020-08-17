/*
  Copyright 2020 Harold Tay GPLv3
  Basic test of usart.
 */

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/usart.h>
#include <libopencm3/stm32/spi.h>
#include <libopencm3/stm32/flash.h>
#include <libopencm3/stm32/f0/nvic.h>
#include <libopencm3/stm32/exti.h>
#include "tx.h"
#include "fmt.h"
#include "usart_setup.h"

#include "sd2.h"
#define MHZ 48
#include "delay.h"                    /* spin delay */

#define LED_PORT GPIOB
#define LED_BIT  GPIO12

#define TX_PORT   GPIOA
#define TX_BIT    GPIO9
#define TX_AF     GPIO_AF1

#define RX_PORT   GPIOA
#define RX_BIT    GPIO10
#define RX_AF     GPIO_AF1

static void led_setup(void)
{
  rcc_periph_clock_enable(RCC_GPIOB);
  gpio_mode_setup(LED_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, LED_BIT);
}

#if 0
static void usart_setup(void)
{
  rcc_periph_clock_enable(RCC_USART1);
  gpio_mode_setup(TX_PORT, GPIO_MODE_AF, GPIO_PUPD_NONE, TX_BIT);
  gpio_set_af(TX_PORT, TX_AF, TX_BIT);
  gpio_mode_setup(RX_PORT, GPIO_MODE_AF, GPIO_PUPD_NONE, RX_BIT);
  gpio_set_af(RX_PORT, TX_AF, RX_BIT);
  usart_set_baudrate(USART1, 57600);
  usart_set_databits(USART1, 8);
  usart_set_parity(USART1, USART_PARITY_NONE);
  usart_set_stopbits(USART1, USART_CR2_STOPBITS_1);
  usart_set_mode(USART1, USART_MODE_TX_RX);
  usart_set_flow_control(USART1, USART_FLOWCONTROL_NONE);
  usart_enable(USART1);
}
#endif

int
main(void)
{
  int i;

  rcc_clock_setup_in_hse_8mhz_out_48mhz();
  led_setup();
  usart_setup(USART1, TX_PORT, TX_BIT, TX_AF, 57600);

  for (i = 4; i > 0; i--) {
    if (i & 1) gpio_set(LED_PORT, LED_BIT);
    else gpio_clear(LED_PORT, LED_BIT);
    tx_msg("Delay ", i);
    delay_ms(800);
  }

  for ( ; ; ) {
    uint16_t ch;
    ch = usart_recv_blocking(USART1);
    usart_send_blocking(USART1, ch);
  }
}
