/*
  Copyright 2020 Harold Tay LGPLv3
  Initialise I2C (ostensibly for rtc operations)
 */
#include <libopencm3/stm32/i2c.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <string.h>
#include "rtc.h"
#include "i2c2.h"

#undef DBG_RTC_I2C
#ifdef DBG_RTC_I2C
#include "tx.h"
#define dbg(x) x
#else
#define dbg(x) /* nothing */
#endif

int8_t rtc_i2c_init(uint32_t i2c_dev,
                    uint32_t gpio_port,
                    uint16_t gpio_sda_scl, uint8_t gpio_af)
{
  do {
#ifdef I2C1
    if (i2c_dev == I2C1) { rcc_periph_clock_enable(RCC_I2C1); break; }
#endif
#ifdef I2C2
    if (i2c_dev == I2C2) { rcc_periph_clock_enable(RCC_I2C2); break; }
#endif
    return(-1);
  } while (0);

  do {
#ifdef GPIOA
    if (gpio_port == GPIOA) { rcc_periph_clock_enable(RCC_GPIOA); break; }
#endif
#ifdef GPIOB
    if (gpio_port == GPIOB) { rcc_periph_clock_enable(RCC_GPIOB); break; }
#endif
#ifdef GPIOC
    if (gpio_port == GPIOC) { rcc_periph_clock_enable(RCC_GPIOC); break; }
#endif
#ifdef GPIOD
    if (gpio_port == GPIOD) { rcc_periph_clock_enable(RCC_GPIOD); break; }
#endif
#ifdef GPIOE
    if (gpio_port == GPIOE) { rcc_periph_clock_enable(RCC_GPIOE); break; }
#endif
#ifdef GPIOF
    if (gpio_port == GPIOF) { rcc_periph_clock_enable(RCC_GPIOF); break; }
#endif
    return(-1);
  } while (0);

  dbg(tx_puts("rtc_i2c calling rcc_set_i2c_clock_hsi()\r\n"));
  rcc_set_i2c_clock_hsi(i2c_dev);  /* a nop? XXX */

  dbg(tx_puts("rtc_i2c calling i2c_reset()\r\n"));
  i2c_reset(i2c_dev);

  gpio_mode_setup(gpio_port, GPIO_MODE_AF, GPIO_PUPD_NONE, gpio_sda_scl);
  gpio_set_af(gpio_port, gpio_af, gpio_sda_scl);

  i2c_peripheral_disable(i2c_dev);
  i2c_enable_analog_filter(i2c_dev);
  i2c_set_digital_filter(i2c_dev, 0);
  i2c_set_speed(i2c_dev, i2c_speed_sm_100k,
    rcc_apb1_frequency/1000000);
  i2c_enable_stretching(i2c_dev);
  i2c_set_7bit_addr_mode(i2c_dev);
  i2c_peripheral_enable(i2c_dev);
  dbg(tx_puts("rtc_i2c returning\r\n"));
  return(0);
}
