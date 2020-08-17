/*
  Copyright 2020 Harold Tay LGPLv3
  Used for logging to determine if a new timestamp is needed.
  We only have 1-second resolution on time stamps.
 */

#include <libopencm3/stm32/timer.h>
#include <libopencm3/stm32/rcc.h>
#include "tick.h"

#undef TICK_DBG
#ifdef TICK_DBG
#include "tx.h"
#define dbg(x) x
#else
#define dbg(x) /* nothing */
#endif

static uint32_t old_value;
void tick_init(void)
{
  rcc_periph_clock_enable(RCC_TIM2);
  rcc_periph_reset_pulse(RST_TIM2);

  timer_set_mode(TIM2, TIM_CR1_CKD_CK_INT,
    TIM_CR1_CMS_EDGE, TIM_CR1_DIR_UP);
  timer_set_prescaler(TIM2, 65535);
  timer_disable_preload(TIM2);
  timer_continuous_mode(TIM2);
  timer_set_period(TIM2, 65535);
  timer_enable_counter(TIM2);
  old_value = 0xffffffff;
}

bool tick_need_stamp(void)
{
#if 1
  static uint32_t new_value;
  new_value = timer_get_counter(TIM2) >> 9;
  dbg(tx_putdec(new_value));
  dbg(tx_puts("\r\n"));
  if (new_value == old_value) return(false);
  old_value = new_value;
  return(true);
#else
  return(true);
#endif
}
