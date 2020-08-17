/*
  Copyright 2020 Harold Tay LGPLv3
  LED for user attention.
 */
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include "attn.h"

#define ATTN_RCC  RCC_GPIOA
#define ATTN_PORT GPIOA
#define ATTN_BIT  GPIO8

/* LED is active low */
void attn_on(void) { gpio_clear(ATTN_PORT, ATTN_BIT); }
void attn_off(void) { gpio_set(ATTN_PORT, ATTN_BIT); }
void attn_init(void)
{
  rcc_periph_clock_enable(ATTN_RCC);
  gpio_mode_setup(ATTN_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, ATTN_BIT);
  attn_off();
}
