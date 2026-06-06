#include "leds.h"
#include "main.h"


void init_led(void)
{
	RCC->AHBENR|=RCC_AHBENR_GPIOBEN;
	GPIOB->MODER&=~GPIO_MODER_MODER3;			//PB3 reset
	GPIOB->MODER|=GPIO_MODER_MODER3_0;		//PB3 output for led
	
}
