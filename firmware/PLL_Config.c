#include <stm32f3xx.h>

//******************************************************************************
//*            PLL (clocked by HSI) used as System clock source                *
//******************************************************************************

void PLL_Config(void)
{
    RCC->APB1ENR |= RCC_APB1ENR_PWREN;	//enable power interface clock source

    RCC->CFGR |= RCC_CFGR_HPRE_DIV1;		//CORE CLOCK
    
    // PCLK2 = HCLK / 2
    RCC->CFGR |= RCC_CFGR_PPRE2_DIV2;		//PERIPHERAL CLOCK 2 = 168MHZ/2 = 42MHZ, THIS IS BECAUSE THE SPI MODULES (AND POSSIBLY OTHERS) DO NOT OPERATE PROPERLY WHEN PCLK > 42MHZ
    
    // PCLK1 = HCLK / 4
    RCC->CFGR |= RCC_CFGR_PPRE1_DIV2;		//PERIPHERAL CLOCK 1 = 168MHZ/2 = 42MHZ, THIS IS BECAUSE THE SPI MODULES (AND POSSIBLY OTHERS) DO NOT OPERATE PROPERLY WHEN PCLK > 42MHZ

    // Configure the main PLL (PLL source is HSE w PREDIV, PLL multiplication is x16, PREDIV is /1)
    RCC->CFGR |= RCC_CFGR_PLLMUL;

    // Enable the main PLL
    RCC->CR |= RCC_CR_PLLON;

    // Wait till the main PLL is ready
    while(!(RCC->CR & RCC_CR_PLLRDY));
   
    // Configure Flash prefetch, Instruction cache, Data cache and wait state
    FLASH->ACR |= FLASH_ACR_LATENCY_1;

    // Select the main PLL as system clock source
    RCC->CFGR |= RCC_CFGR_SW_PLL;

    // Wait till the main PLL is used as system clock source
    while ((RCC->CFGR & RCC_CFGR_SWS ) != RCC_CFGR_SWS_PLL);
}

//******************************************************************************
//*            END PLL (CLOCKED BY HSI) SETUP CODE                             *
//******************************************************************************
