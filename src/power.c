/*
  Copyright 2020 Harold Tay LGPLv3
  Powers on/off SD card and VDD of PCM1808 (but not VCC of PCM1808).
 */
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include "power.h"
#define MHZ 48
#include "delay.h"
/*
  GPIO defs
 */
#define POWER_RCC   RCC_GPIOB
#define POWER_PORT  GPIOB
#define POWER_BIT   GPIO6

#define POWER_MODE_RCC RCC_GPIOB
#define POWER_MODE_PORT GPIOB
#define POWER_MODE_BIT  GPIO7

void power_on(uint8_t mode)
{
  rcc_periph_clock_enable(POWER_RCC);
  gpio_mode_setup(POWER_PORT, GPIO_MODE_OUTPUT,
    GPIO_PUPD_NONE, POWER_BIT);
  rcc_periph_clock_enable(POWER_MODE_RCC);
  gpio_mode_setup(POWER_MODE_PORT, GPIO_MODE_OUTPUT,
    GPIO_PUPD_NONE, POWER_MODE_BIT);

  if (POWER_MODE_SLAVE == mode)
    gpio_set(POWER_MODE_PORT, POWER_MODE_BIT);
  else
    gpio_clear(POWER_MODE_PORT, POWER_MODE_BIT);
  gpio_set(POWER_PORT, POWER_BIT);
  delay_ms(100);
}

void power_off(void)
{
  gpio_clear(POWER_PORT, POWER_BIT);
}
