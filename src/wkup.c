/*
  Copyright 2020 Harold Tay LGPLv3
  Wake up from external source.
  Interrupt line is on PB2.
  - Configure mask bit in EXTI_IMR
  - Configure trigger in EXTI_RTSR and EXTI_FTSR
  - Configure enable/mask bits 
 */

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/f0/nvic.h>
#include <libopencm3/stm32/exti.h>
#include "wkup.h"

#undef DBG
#ifdef DBG
#include "tx.h"
#define dbg(x) x
#else
#define dbg(x) /* nothing */
#endif

volatile uint8_t wkup_flag;

void WKUP_ISR(void)
{
  EXTI_PR = WKUP_EXTI;                /* Reset interrupt flag */
  if (!(GPIO_IDR(WKUP_PORT) & WKUP_BIT)) {
    wkup_flag = 1;
    dbg(tx_putc('!'));
  }
}

void wkup_init(void)
{
  rcc_periph_clock_enable(WKUP_RCC);
  gpio_mode_setup(WKUP_PORT, GPIO_MODE_INPUT, GPIO_PUPD_PULLUP, WKUP_BIT);
}

void wkup_enable(void)
{
  nvic_enable_irq(WKUP_IRQ);
  exti_select_source(WKUP_EXTI, WKUP_PORT); /* SYSCFG_EXTICR */
  exti_set_trigger(WKUP_EXTI, EXTI_TRIGGER_FALLING);  /* EXTI_RTSR */
  exti_enable_request(WKUP_EXTI);  /* bit set in EXTI_IMR */
}

void wkup_disable(void)
{
  exti_disable_request(WKUP_EXTI);
}
