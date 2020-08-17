/*
  Copyright 2020 Harold Tay GPLv3
  Working out the kinks in getting interrupts to work.
  Manually shorting INT pin to gnd.
 */

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/f0/nvic.h>
#include <libopencm3/stm32/exti.h>
#include "usart_setup.h"
#include "tx.h"
#include "fmt.h"
#define MHZ 48
#include "delay.h"

#if 0
#define INT_RCC  RCC_GPIOB
#define INT_IRQ  NVIC_EXTI2_3_IRQ
#define INT_EXTI EXTI2
#define INT_PORT GPIOB
#define INT_BIT  GPIO2
#define INT_ISR  exti2_3_isr
#else
#define INT_RCC  RCC_GPIOA
#define INT_IRQ  NVIC_EXTI0_1_IRQ
#define INT_EXTI EXTI0
#define INT_PORT GPIOA
#define INT_BIT  GPIO0
#define INT_ISR  exti0_1_isr
#endif

volatile static uint8_t interrupted;
void INT_ISR(void)
{
  /* if ((gpio_port_read(GPIOB) & GPIO2)) return; */
  exti_reset_request(INT_EXTI);
  interrupted = 1;
}

static void int_init(void)
{
  rcc_periph_clock_enable(INT_RCC);
  nvic_enable_irq(INT_IRQ);
  nvic_set_priority(INT_IRQ, 0);
  gpio_mode_setup(INT_PORT, GPIO_MODE_INPUT, GPIO_PUPD_PULLUP, INT_BIT);
  exti_select_source(INT_EXTI, INT_PORT);
  exti_set_trigger(INT_EXTI, EXTI_TRIGGER_BOTH);
  exti_enable_request(INT_EXTI);
}

int
main(void)
{
  int i;
  uint8_t state;

  rcc_clock_setup_in_hse_8mhz_out_48mhz();
  usart_setup(USART1, GPIOA, GPIO9, GPIO_AF1, 57600);
  int_init();

  for (i = 4; i > 0; i--) {
    tx_msg("Delay ", i);
    delay_ms(900);
  }

#define READ_BIT (!!(gpio_port_read(INT_PORT) & INT_BIT))
  state = READ_BIT;
  for ( ; ; ) {
    uint8_t new_state;
#if 1
    if (!interrupted) continue;
    interrupted = 0;
#endif
    new_state = READ_BIT;
    if (new_state != state) {
      tx_msg("INT is ", new_state);
      state = new_state;
    }
  }
}
