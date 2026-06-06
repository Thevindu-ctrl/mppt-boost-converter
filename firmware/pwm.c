/*
 * ============================================================================
 * pwm.c  —  100 kHz PWM output driver for MPPT boost converter gate drive
 * ============================================================================
 *
 * TARGET:  STM32F303K8T @ 64 MHz
 *
 * HARDWARE PATH
 * ─────────────────────────────────────────────────────────────────────────────
 *  STM32 PB0 (TIM3_CH3, AF2) → 56Ω R_Gate → TC4429 gate driver (12V supply)
 *                                           → IRF510 N-channel MOSFET gate
 *
 *  The 56Ω gate resistor was empirically selected during lab testing to
 *  critically damp the LC tank formed by parasitic breadboard trace inductance
 *  and MOSFET input capacitance (C_iss ≈ 180pF for IRF510).
 *
 * TIMER CONFIGURATION
 * ─────────────────────────────────────────────────────────────────────────────
 *  Timer        : TIM3 (APB1 bus, max clock = PCLK1)
 *  Channel      : CH3 (PB0, alternate function AF2)
 *  Mode         : PWM Mode 1 (OC3M = 110b)
 *                 Output HIGH when TIM3_CNT < TIM3_CCR3
 *                 Output LOW  when TIM3_CNT ≥ TIM3_CCR3
 *  Prescaler    : 1 (PSC = 0, no division — maximises resolution)
 *  Auto-reload  : ARR = SystemCoreClock / F_pwm
 *                 At 100 kHz: ARR = 64,000,000 / 100,000 = 640 counts
 *                 Duty resolution: 100% / 640 steps ≈ 0.156% per count
 *
 * DUTY CYCLE RESOLUTION NOTE
 * ─────────────────────────────────────────────────────────────────────────────
 *  The P&O algorithm uses 0.5% steps which maps to 3.2 timer counts.
 *  This is coarser than the hardware resolution, so no quantisation issues.
 * ============================================================================
 */

#include <stm32f3xx.h>
#include "pwm.h"

/* PWM_MAX is the ARR value — stored globally so output_pwm() can scale
 * a 0–100% float input to the correct compare register value.           */
unsigned short PWM_MAX;

/* ===========================================================================
 * init_pwm()
 *
 * Configures TIM3 CH3 on PB0 for PWM output at the specified frequency.
 *
 * @param F_pwm  Desired PWM frequency in Hz (use 100000 for 100 kHz)
 * ===========================================================================*/
void init_pwm(unsigned int F_pwm)
{
    /* ── PB0: Alternate Function mode, AF2 = TIM3_CH3 ──────────────────────*/
    #define AF_TIM3  2u

    RCC->AHBENR |= RCC_AHBENR_GPIOBEN;              /* enable GPIOB clock    */

    GPIOB->MODER &= ~GPIO_MODER_MODER0;             /* clear PB0 mode bits   */
    GPIOB->MODER |=  GPIO_MODER_MODER0_1;           /* set AF mode (10b)     */

    GPIOB->AFR[0] &= ~GPIO_AFRL_AFRL0;              /* clear AFRL0 bits      */
    GPIOB->AFR[0] |=  (AF_TIM3 << GPIO_AFRL_AFRL0_Pos); /* AF2 = TIM3       */

    /* ── TIM3 clock and prescaler ───────────────────────────────────────────*/
    RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;             /* enable TIM3 clock     */

    /*
     * PSC = 1 → timer counts at SystemCoreClock / 1 = 64 MHz.
     * ARR = SystemCoreClock / (PSC × F_pwm) — one PWM period.
     *
     * Note: The skeleton uses psc3=1 (i.e. PSC register = psc3-1 = 0).
     * Written as unsigned short but ARR at 100 kHz = 640, safely fits.
     */
    unsigned short psc3 = 1u;
    unsigned short arr3 = (unsigned short)(SystemCoreClock / (psc3 * F_pwm));

    /* ── TIM3 CH3: PWM Mode 1 (OC3M = 110b in CCMR2) ──────────────────────*/
    /*
     * CCMR2 register controls channels 3 and 4.
     * OC3M[2:0] = 110 selects PWM Mode 1 for CH3.
     * CCMR2 is fully written here (no read-modify-write) so OC3PE and other
     * bits are left at reset values which is fine for basic PWM operation.
     */
    TIM3->CCMR2 = (6u << TIM_CCMR2_OC3M_Pos);

    /* ── Enable CH3 compare output ─────────────────────────────────────────*/
    TIM3->CCER |= TIM_CCER_CC3E;                     /* CC3E: output enable  */

    /* ── Load prescaler and period ──────────────────────────────────────────*/
    TIM3->PSC = psc3 - 1;                            /* prescaler value       */
    TIM3->ARR = arr3;                                /* auto-reload (period)  */
    TIM3->CNT = 0;                                   /* reset counter         */

    /* ── Store ARR for use in output_pwm() ─────────────────────────────────*/
    PWM_MAX = arr3;

    /* ── Start the timer ────────────────────────────────────────────────────*/
    TIM3->CR1 |= TIM_CR1_CEN;                        /* counter enable        */
}

/* ===========================================================================
 * output_pwm()
 *
 * Sets the duty cycle of TIM3 CH3 by writing to CCR3.
 *
 * @param d  Duty cycle in percent (0.0–100.0). Values outside this range
 *           are clamped. The P&O algorithm enforces 10–90% limits before
 *           calling this function, so the clamp here is a safety backstop.
 *
 * CCR3 = d% × ARR / 100
 * Example: d=50.0, ARR=640 → CCR3=320 → 50% duty cycle at PB0
 * ===========================================================================*/
void output_pwm(float d)
{
    if (d > 100.0f) d = 100.0f;
    if (d <   0.0f) d =   0.0f;

    TIM3->CCR3 = (unsigned short)(d * (float)PWM_MAX / 100.0f);
}
