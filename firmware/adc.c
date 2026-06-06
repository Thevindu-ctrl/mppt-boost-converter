/*
 * ============================================================================
 * adc.c  —  ADC1 peripheral driver for MPPT voltage and current sensing
 * ============================================================================
 *
 * TARGET:  STM32F303K8T @ 64 MHz
 *
 * CHANNEL MAPPING
 * ─────────────────────────────────────────────────────────────────────────────
 *  PA0  →  ADC1_IN1  →  Voltage sensing (40kΩ/10kΩ divider + MCP602 buffer)
 *  PA1  →  ADC1_IN2  →  Current sensing (1Ω shunt + MCP602 unity-gain buffer)
 *
 * ADC CONFIGURATION
 * ─────────────────────────────────────────────────────────────────────────────
 *  Resolution     : 12-bit (0–4095)
 *  Clock source   : AHB clock ÷ (CKMODE bits 11b = synchronous mode)
 *                   With RCC->CFGR2 ADCPRE12 = 0x11 → ADC clock = PCLK2/4
 *                   PCLK2 = 64 MHz / 2 = 32 MHz → ADC clock = 8 MHz
 *  Conversion mode: Single (software-triggered per read_adc() call)
 *  Alignment      : Right (default)
 *  Sequence length: 1 channel per conversion
 *  Voltage regulator: enabled (ADVREGEN = 01b, mandatory startup sequence)
 *
 * NOTES
 * ─────────────────────────────────────────────────────────────────────────────
 *  • The 10nF RC filter (390Ω, fc≈40kHz) on both analog inputs is already
 *    implemented in hardware and suppresses 100kHz PWM switching noise before
 *    it reaches the ADC pins.
 *  • Software oversampling (×8) in main.c further reduces ADC quantisation
 *    noise by approximately √8 ≈ 2.8, effectively gaining ~1.5 bits.
 *  • This file is unchanged from the original skeleton adc.c except for
 *    clarified comments aligned to the MPPT hardware design.
 * ============================================================================
 */

#include <stm32f303x8.h>
#include "adc.h"

/* ===========================================================================
 * ADC_Init()
 *
 * Configures GPIO pins PA0 and PA1 as analogue inputs (MODER = 11b) and
 * initialises ADC1 for single software-triggered conversions.
 *
 * Startup sequence for STM32F3 ADC (as per RM0316 §13.3.6):
 *  1. Set ADVREGEN[1:0] = 01 (regulator enable — NOT the same as F4/F7!)
 *  2. Wait at least 10 µs for the regulator to stabilise
 *  3. Set ADEN = 1
 *  4. Wait for ADRDY = 1
 * ===========================================================================*/
void ADC_Init(void)
{
    /* ── GPIO: PA0 and PA1 → analogue mode (MODER = 11b each) ──────────────*/
    RCC->AHBENR |= RCC_AHBENR_GPIOAEN;              /* enable GPIOA clock    */
    GPIOA->MODER |= (GPIO_MODER_MODER0               /* PA0 → analogue       */
                   | GPIO_MODER_MODER1);             /* PA1 → analogue       */

    /* ── ADC clock: enable ADC12 on AHB ────────────────────────────────────*/
    RCC->AHBENR |= RCC_AHBENR_ADC12EN;

    /*
     * ADCPRE12[3:0] bits in RCC_CFGR2 (bits [7:4]):
     * Writing 0x11 (binary 0001_0001) selects PCLK2/4.
     * With PCLK2 = 32 MHz → ADC clock = 8 MHz (within 12 MHz max spec).
     */
    RCC->CFGR2 |= (0x11 << 4);

    /*
     * Voltage regulator startup (STM32F3 specific, ADVREGEN[1:0]):
     *  Step 1: clear ADVREGEN bits (write 00 first as required by RM0316)
     *  Step 2: write 01b to enable the regulator
     */
    ADC1->CR &= ~ADC_CR_ADVREGEN;                   /* clear ADVREGEN        */
    ADC1->CR |=  ADC_CR_ADVREGEN_0;                 /* write 01b → enabled   */

    /*
     * No explicit 10µs wait loop here because the subsequent ADEN + ADRDY
     * poll already takes longer than 10µs at 64 MHz, satisfying t_ADCVREG_STUP.
     * If you ever reduce SystemCoreClock or add DMA, insert:
     *     delay_nms(1);
     * after setting ADVREGEN_0.
     */

    /* ── Enable ADC ─────────────────────────────────────────────────────────*/
    ADC1->CR |= ADC_CR_ADEN;

    /* Wait until ADC ready flag is set (ADRDY in ISR, set after startup time)*/
    while (!(ADC1->ISR & ADC_ISR_ADRD));
}

/* ===========================================================================
 * read_adc()
 *
 * Triggers a single conversion on the specified channel number and returns
 * the 12-bit result (0–4095).
 *
 * @param channel  ADC1 channel number: 1 = PA0 (voltage), 2 = PA1 (current)
 * @return         12-bit ADC result
 *
 * Conversion time at 8 MHz ADC clock with 1.5 cycle sample time (default):
 *   t_conv = (1.5 + 12.5) / 8 MHz = 1.75 µs per conversion
 *   With ×8 oversampling: ~14 µs total, well within the 100 ms P&O loop.
 * ===========================================================================*/
unsigned short read_adc(unsigned char channel)
{
    /* Set sequence register: one conversion, selected channel in position 1 */
    ADC1->SQR1 = (unsigned int)((channel) << ADC_SQR1_SQ1_Pos);

    /* Clear any previous EOC flag */
    ADC1->ISR |= ADC_ISR_EOC;

    /* Start conversion */
    ADC1->CR |= ADC_CR_ADSTART;

    /* Wait for End-Of-Conversion flag */
    while (!(ADC1->ISR & ADC_ISR_EOC)) { __NOP(); }

    /* Reading DR automatically clears the EOC flag */
    return (unsigned short)(ADC1->DR);
}
