/*
  Copyright 2020 Harold Tay GPLv3
  Basic I2C testing
 */

#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/i2c.h>
#include <libopencm3/stm32/usart.h>
#include "usart_setup.h"
#include "tx.h"
#define MHZ 48
#include "delay.h"

int
main(void)
{
  int i;
  uint8_t cmd[7], res[7];

  rcc_clock_setup_in_hse_8mhz_out_48mhz();
  usart_setup(TX_USART, TX_PORT, TX_BIT, TX_AF, 57600);
  for (i = 4; i > 0; i--) {
    tx_msg("Delay ", i);
    delay_ms(800);
  }

  tx_puts("rcc_apb1_frequency=");
  tx_putdec32(rcc_apb1_frequency);
  tx_puts("\r\n");

  tx_puts("Set up clocks\r\n");
  rcc_periph_clock_enable(RCC_GPIOB);
  rcc_periph_clock_enable(RCC_I2C1);
  gpio_mode_setup(GPIOB, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO8|GPIO9);
  gpio_set_af(GPIOB, GPIO_AF1, GPIO8|GPIO9);

  /* Figure 217 */
  I2C_CR1(I2C1) &= ~I2C_CR1_PE;
  I2C_CR1(I2C1) &= ~I2C_CR1_ANFOFF;   /* enable analogue filter */
  /* Disable digital filter */
  I2C_CR1(I2C1) &= ~(I2C_CR1_DNF_MASK << I2C_CR1_DNF_SHIFT);
  I2C_TIMINGR(I2C1) &= 0x0f000000;
  I2C_TIMINGR(I2C1) |= ((48/4)-1) << I2C_TIMINGR_PRESC_SHIFT;
  I2C_TIMINGR(I2C1) |= 19 << I2C_TIMINGR_SCLL_SHIFT;
  I2C_TIMINGR(I2C1) |= 15 << I2C_TIMINGR_SCLH_SHIFT;
  I2C_TIMINGR(I2C1) |= 2 << I2C_TIMINGR_SDADEL_SHIFT;
  I2C_TIMINGR(I2C1) |= 4 << I2C_TIMINGR_SCLDEL_SHIFT;
  I2C_CR1(I2C1) &= ~I2C_CR1_NOSTRETCH;
  I2C_CR2(I2C1) &= ~I2C_CR2_ADD10;
  I2C_CR1(I2C1) |= I2C_CR1_PE;

  tx_puts("I2C all set up, sending to i2c\r\n");
  /*
    Presumably all set up.
    Get ds3231 status.
   */

  I2C_CR2(I2C1) |= 0x68;              /* set slave address */
  I2C_CR2(I2C1) &= ~(I2C_CR2_RD_WRN); /* write direction */
  I2C_CR2(I2C1) |= (1<<I2C_CR2_NBYTES_SHIFT);
  I2C_CR2(I2C1) &= ~(I2C_CR2_AUTOEND); /* disable autoend */
  I2C_CR2(I2C1) |= I2C_CR2_START;     /* send start */

  tx_puts("Entering send loop\r\n");
  *cmd = 0x0e;                        /* control register */
  for (i = 0; ; ) {
    if ((I2C_ISR(I2C1) & I2C_ISR_TXIS)) {
      tx_puts("TXIS set\r\n");
      I2C_TXDR(I2C1) = cmd[i++];      /* send data */
      if (i >= 1) break;
    }
    if (I2C_ISR(I2C1) & I2C_ISR_NACKF) {
      tx_puts("Error:NACKF set\r\n");
      break;
    }
  }

done:
  I2C_CR2(I2C1) |= I2C_CR2_STOP;
  tx_puts("Stop sent\r\n");
  for ( ; ; );
}
