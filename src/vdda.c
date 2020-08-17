/*
  Copyright 2020 Harold Tay LGPLv3
  Read Vdd using vrefint.

  CONT=0: Single conversion.  A sequence is started (by setting
  ADSTART for example) and ends when the sequence is completed.
  CONT=1: Continuous conversion.  The sequence is repeatedly
  converted.  Use ADSTP to stop (or some other method?)

  Sequences are the set of channels to convert, set in
  ADC_CHSELR.
 */

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/adc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/f0/memorymap.h>  /* for ST_VREFINT_CAL */
#include "vdda.h"

void vdda_init(void)
{
  uint8_t vrefint;
  rcc_periph_clock_enable(RCC_ADC);
  adc_power_off(ADC1);
  adc_set_clk_source(ADC1, ADC_CLKSOURCE_ADC);
  adc_calibrate(ADC1);
  adc_set_right_aligned(ADC1);
  adc_set_single_conversion_mode(ADC1);
  adc_disable_external_trigger_regular(ADC1);
  adc_enable_vrefint();
  vrefint = ADC_CHANNEL_VREF;
  adc_set_regular_sequence(ADC1, 1, &vrefint);
  adc_set_resolution(ADC1, ADC_RESOLUTION_12BIT);
  adc_disable_analog_watchdog(ADC1);
  adc_power_on(ADC1);
}

int16_t vdda_read_mv(void)
{
  uint16_t code;
  adc_start_conversion_regular(ADC1);
  while (!(adc_eoc(ADC1)));

  code = adc_read_regular(ADC1);
  return((3300 * ST_VREFINT_CAL)/code);
}
