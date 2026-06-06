/*
 * ============================================================================
 * adc.h  —  ADC driver interface for MPPT voltage and current sensing
 * ============================================================================
 *
 * CHANNEL MACROS
 * ─────────────────────────────────────────────────────────────────────────────
 *  convert_PA0  →  reads ADC1_IN1 (PA0) — voltage sense
 *                  Raw value 0–4095 maps to 0–3.3V at ADC pin
 *                  Multiply by (3.3/4095 × 5.0) to get V_panel in Volts
 *
 *  convert_PA1  →  reads ADC1_IN2 (PA1) — current sense
 *                  Raw value 0–4095 maps to 0–3.3V at ADC pin
 *                  Multiply by (3.3/4095 / 1.0) to get I_panel in Amps
 * ============================================================================
 */

#ifndef _ADC_H
#define _ADC_H

/* Convenience macros matching the original skeleton API.
 * Channel numbers map to STM32F303K8 ADC1 pin assignments:
 *   ADC1_IN1 = PA0,  ADC1_IN2 = PA1  (see RM0316 Table 56)             */
#define convert_PA0    read_adc(1)   /* voltage divider output → V_panel */
#define convert_PA1    read_adc(2)   /* current shunt output   → I_panel */

void           ADC_Init(void);
unsigned short read_adc(unsigned char channel);

#endif /* _ADC_H */
