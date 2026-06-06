/*
 * ============================================================================
 * pwm.h  —  100 kHz PWM output driver interface
 * ============================================================================
 *
 * Output: PB0 (TIM3_CH3, AF2)
 * Usage:
 *   init_pwm(100000);      // configure 100 kHz
 *   output_pwm(50.0f);     // set 50% duty cycle
 *   output_pwm(s_duty);    // update from P&O algorithm
 * ============================================================================
 */

#ifndef _PWM_H__
#define _PWM_H__

void init_pwm(unsigned int F_pwm);
void output_pwm(float d);

/* Timer auto-reload register value — set by init_pwm(), used by output_pwm().
 * At 100 kHz with 64 MHz clock: PWM_MAX = 640 (period = 640 timer counts)   */
extern unsigned short PWM_MAX;

#endif /* _PWM_H__ */
