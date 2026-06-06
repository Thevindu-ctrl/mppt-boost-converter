#ifndef _MAIN_H
#define _MAIN_H

#include <math.h>
#include <stm32f3xx.h>
#define Fcy		64000000

#define ADC_Interrupt_Priority 						0x40
#define Sampling_Timer_Interrupt_Priority	0x30
#define USART_Timer_Interrupt_Priority		0x20

void PLL_Config(void);

#endif
