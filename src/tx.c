/*
  Copyright 2020 Harold Tay LGPLv3
  Interrupt driven serial (uart) output.
 */

#include "tx.h"
#include "fmt.h"

#ifdef __AVR__

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>             /* for tx_puthex32 */
#ifndef USART_BPS
#define USART_BPS 57600
#endif
void tx_putc(char ch) { while( !(UCSR0A&_BV(UDRE0)) ); UDR0=ch; }

#else

#define PROGMEM /* nothing */
#define pgm_read_byte_near(x) *((x))
#include <libopencm3/stm32/usart.h>
void tx_putc(char ch) { usart_send_blocking(USART1, ch); }

#endif

void tx_puts(char * s) { while( *s )tx_putc(*s++); }

void tx_init(void)
{
#ifdef __AVR__
  UBRR0 = ((F_CPU/8)/USART_BPS) - 1;
  UCSR0B |= _BV(TXEN0);
  UCSR0A |= _BV(U2X0);
#endif
}

void tx_putdec(int16_t d) { tx_puts(fmt_i16d((int16_t)d)); }
void tx_putdec32(int32_t d) { tx_puts(fmt_i32d(d)); }
void tx_puthex(uint8_t x) { tx_puts(fmt_x(x)); }
void tx_puthex32(uint32_t x) { tx_puts(fmt_32x(x)); }
void tx_msg(char * s, int16_t d)
{
  tx_puts(s);
  tx_putdec(d);
  tx_puts("\r\n");
}
void tx_putpgms(const char * s)
{
  char ch;
  for ( ; ; s++) {
    ch = pgm_read_byte_near(s);
    if (!ch) break;
    tx_putc(ch);
  }
}
