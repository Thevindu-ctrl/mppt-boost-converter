#include "usart.h"
#include "main.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void init_usart(unsigned int baud)
{
	RCC->AHBENR|=RCC_AHBENR_GPIOAEN;
	GPIOA->MODER&=~(GPIO_MODER_MODER2|GPIO_MODER_MODER3);
	GPIOA->MODER|=(GPIO_MODER_MODER2_1|GPIO_MODER_MODER3_1);
	GPIOA->AFR[0]&=~0xff00;
	GPIOA->AFR[0]|=0x7700;
	
	RCC->APB1ENR|=RCC_APB1ENR_USART2EN;
	USART2->CR1|=(
			USART_CR1_UE		//usart enabled
			|USART_CR1_TE		//transmitter enabled
			);
	USART2->BRR=Fcy/(2*baud);
	
	print_terminal("USART Ready!!\n\r");
}

void print_terminal(const char *s)
{
	for (unsigned char i=0; i<strlen(s); i++) send_usart(s[i]);
}

void send_usart(unsigned char d)
{
	while(!(USART2->ISR&USART_ISR_TXE));
	USART2->TDR=d;
}

