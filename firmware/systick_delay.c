#include <stm32f3xx.h>
#include "systick_delay.h"
#include "PWR.h"

#define ms_count	(SystemCoreClock/1000)
unsigned int ms_counter=0;
void SysTick_Init(void) 
{
	
	while (SysTick_Config(ms_count) != 0);
	NVIC_SetPriority(SysTick_IRQn,0);
}


void delay_nms(unsigned int n)     //delay of n milliseconds
{
	unsigned int ms_start=ms_counter;
	while((unsigned int)(ms_counter-ms_start)<n)
	{
		Sleep();
	}
}

void SysTick_Handler(void)
{
	ms_counter++;
}

