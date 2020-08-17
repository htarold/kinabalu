#ifndef WKUP_H
#define WKUP_H
/*
  Copyright 2020 Harold Tay LGPLv3
  Wake up when the wkup pin is brought down.
 */
extern volatile uint8_t wkup_flag;
extern void exti2_3_isr(void);
extern void wkup_init(void);
extern void wkup_enable(void);
extern void wkup_disable(void);

#define WKUP_RCC   RCC_GPIOA
#define WKUP_PORT  GPIOA
#define WKUP_BIT   GPIO0
#define WKUP_EXTI  EXTI0
#define WKUP_IRQ   NVIC_EXTI0_1_IRQ
#define WKUP_ISR   exti0_1_isr
#endif /* WKUP_H */
