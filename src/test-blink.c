/*
  Copyright 2020 Harold Tay GPLv3
  Blink LED on PA9
 */

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>

#define MHZ 48
#include "delay.h"                    /* spin delay */

#define LED_PORT GPIOA
#define LED_BIT  GPIO9

static void led_setup(void)
{
  rcc_periph_clock_enable(RCC_GPIOA);
  gpio_mode_setup(LED_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, LED_BIT);
}

int
main(void)
{
  rcc_clock_setup_in_hse_8mhz_out_48mhz();
  led_setup();

  for ( ; ; ) {
    gpio_set(LED_PORT, LED_BIT);
    delay_ms(800);
    gpio_clear(LED_PORT, LED_BIT);
    delay_ms(800);
  }
}
