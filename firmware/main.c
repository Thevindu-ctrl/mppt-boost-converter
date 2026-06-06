/*
 * ============================================================================
 * arm-mppt-boost-converter — main.c
 * PUSL3198 Design and Control of Renewable Energy Technology
 * W.L.P.T.N. Wijayasinghe — 10955873
 * ============================================================================
 *
 * TARGET:  STM32F303K8T (Nucleo-32 board), 64 MHz via PLL from HSI
 *
 * HARDWARE MAPPING
 * ─────────────────────────────────────────────────────────────────────────────
 *  Signal          Pin     Description
 *  ─────────────   ─────   ─────────────────────────────────────────────────
 *  V_SENSE         PA0     40kΩ/10kΩ voltage divider output → ADC1_CH1
 *                          20V panel → 4.0V max at divider (5:1), safe for 3.3V
 *                          ADC with 3.3V Zener clamp protection
 *  I_SENSE         PA1     MCP602 unity-gain buffer of 1Ω shunt → ADC1_CH2
 *                          Max panel current ~0.67A → 0.67V max
 *  PWM_OUT         PB0     TIM3_CH3 → 100 kHz gate signal → TC4429 gate driver
 *  STATUS_LED      PB3     Onboard Nucleo-32 green LED (LD3)
 *
 * SIGNAL CONDITIONING CHAIN
 * ─────────────────────────────────────────────────────────────────────────────
 *  Voltage: V_panel → [40kΩ / 10kΩ divider] → [MCP602 buffer] →
 *           [RC LPF 390Ω + 10nF, fc≈40kHz] → [4.7V Zener clamp] → PA0
 *  Scale:   V_sense = V_panel / 5.0
 *  Recover: V_panel = ADC_raw * (3.3 / 4095) * 5.0
 *
 *  Current: V_panel+ → [1Ω shunt] → [MCP602 x1 buffer] →
 *           [RC LPF 390Ω + 10nF] → [4.7V Zener clamp] → PA1
 *  Scale:   V_sense = I_panel * 1.0  (1Ω shunt, unity gain)
 *  Recover: I_panel = ADC_raw * (3.3 / 4095) / 1.0
 *
 * P&O ALGORITHM PARAMETERS
 * ─────────────────────────────────────────────────────────────────────────────
 *  Perturbation step  : 0.5% duty cycle per iteration
 *  Dead-zone threshold: 5 mW  (avoids jitter when settled at MPP)
 *  Loop period        : 100 ms (allows output to settle after each perturbation)
 *  Duty cycle limits  : 10% – 90% (prevents inductor saturation / output OV)
 *
 * POWER LOSS REFERENCE (Chapter 3 of report, at 10W MPP, D=0.5, 100kHz)
 * ─────────────────────────────────────────────────────────────────────────────
 *  MOSFET conduction : 0.121 W  (IRF510, R_DS(on)=0.54Ω)
 *  Diode conduction  : 0.335 W  (1N4002, Vf=1.0V)  ← dominant loss
 *  MOSFET switching  : 0.029 W  (tr=16ns, tf=9.4ns, Coss=81pF)
 *  Core loss (3C90)  : 0.050 W
 *  Estimated η       : 94.65%
 *
 * LED STATUS CODES (PB3 = onboard LED, no external RGB fitted on this board)
 * ─────────────────────────────────────────────────────────────────────────────
 *  Steady ON  : MPP locked (|ΔP| ≤ 5 mW dead-zone)
 *  Slow blink : Tracking toward MPP
 *  Fast blink : Duty limit reached (10% or 90%) — check circuit
 * ============================================================================
 */

#include <stm32f3xx.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "adc.h"
#include "leds.h"
#include "usart.h"
#include "systick_delay.h"
#include "PLL_Config.h"
#include "pwm.h"
#include "main.h"

/* ── P&O algorithm tuneable parameters ─────────────────────────────────────*/
#define PERTURB_STEP_PCT    0.5f   /* duty cycle step per iteration (%)       */
#define POWER_DEADZONE_W    0.005f /* dead-zone: stop oscillating at MPP (W)  */
#define DUTY_MIN_PCT        10.0f  /* lower duty limit — boost cannot go below */
#define DUTY_MAX_PCT        90.0f  /* upper duty limit — prevent OV/saturation */
#define LOOP_DELAY_MS       100    /* P&O iteration period (ms)               */
#define ADC_OVERSAMPLE      8      /* number of ADC reads to average per ch   */

/* ── Hardware scaling factors ───────────────────────────────────────────────*/
#define V_DIVIDER_RATIO     5.0f   /* 40kΩ/(40kΩ+10kΩ) → ×5 to recover Vpanel */
#define I_SHUNT_OHM         1.0f   /* shunt resistor value (Ω)                */
#define I_OPAMP_GAIN        1.0f   /* MCP602 configured as unity-gain buffer  */
#define ADC_VREF            3.3f   /* ADC reference voltage (V)               */
#define ADC_RESOLUTION      4095.0f /* 12-bit ADC (2^12 − 1)                  */

/* ── Algorithm state (persistent across iterations) ────────────────────────*/
static float s_duty_pct  = 50.0f; /* starting duty cycle (%)                 */
static float s_P_prev    = 0.0f;  /* power at previous iteration (W)         */
static float s_V_prev    = 0.0f;  /* voltage at previous iteration (V)       */

/* ── Private function prototypes ────────────────────────────────────────────*/
static float   adc_oversample_voltage(void);
static float   adc_oversample_current(void);
static void    mppt_perturb_and_observe(void);
static void    update_status_led(float dP);

/* ===========================================================================
 * main()
 * Initialises all peripherals then enters the P&O MPPT control loop.
 * ===========================================================================*/
int main(void)
{
    /* ── System clock: HSI × 16 = 64 MHz via PLL ───────────────────────────*/
    PLL_Config();
    SystemCoreClockUpdate();

    /* ── SysTick for delay_nms() — 1 kHz tick ──────────────────────────────*/
    SysTick_Init();

    /* ── USART2 @ 115200 baud (PA2=TX, PA3=RX, USB-UART on Nucleo CN3) ─────*/
    init_usart(115200);

    /* ── ADC1: PA0 (CH1 = voltage), PA1 (CH2 = current) ────────────────────*/
    ADC_Init();

    /* ── PB3: onboard LED (LD3) ─────────────────────────────────────────────*/
    init_led();

    /* ── TIM3 CH3 @ 100 kHz → PB0 PWM output ───────────────────────────────*/
    init_pwm(100000);

    /* ── Set initial duty cycle and let the converter settle ────────────────*/
    output_pwm(s_duty_pct);
    delay_nms(500);

    print_terminal("MPPT Boost Converter — P&O Algorithm Started\r\n");
    print_terminal("V(V)\tI(A)\tP(W)\tDuty(%)\r\n");

    /* ── Main control loop ───────────────────────────────────────────────────*/
    while (1)
    {
        led_on();                    /* LED on during computation             */
        mppt_perturb_and_observe();  /* one P&O iteration                     */
        led_off();
        delay_nms(LOOP_DELAY_MS);   /* wait for converter output to settle   */
    }
}

/* ===========================================================================
 * mppt_perturb_and_observe()
 *
 * Standard two-variable P&O algorithm.
 *
 * Decision table (signs of ΔP and ΔV):
 *   ΔP > 0, ΔV > 0  →  left of MPP, operating point moving right  → decrease D
 *   ΔP > 0, ΔV < 0  →  left of MPP, operating point moving left   → increase D
 *   ΔP < 0, ΔV > 0  →  right of MPP, operating point moving right → increase D
 *   ΔP < 0, ΔV < 0  →  right of MPP, operating point moving left  → decrease D
 *
 * For a boost converter: increasing D raises V_out, reducing V_panel load.
 * The PV panel current source characteristic means:
 *   higher duty → lower V_panel (panel pushed toward I_SC)
 *   lower duty  → higher V_panel (panel pushed toward V_OC)
 * ===========================================================================*/
static void mppt_perturb_and_observe(void)
{
    char msg[80];

    /* ── Step 1: Measure V and I from signal-conditioned ADC inputs ─────────*/
    float V = adc_oversample_voltage();
    float I = adc_oversample_current();
    float P = V * I;

    /* ── Step 2: Compute deltas ──────────────────────────────────────────────*/
    float dP = P - s_P_prev;
    float dV = V - s_V_prev;

    /* ── Step 3: Perturb duty cycle (only outside dead-zone) ────────────────*/
    if (fabsf(dP) > POWER_DEADZONE_W)
    {
        /*
         * Standard P&O: move in direction that previously increased power.
         * dP > 0 → last perturbation helped, keep going same way (same sign as dV).
         * dP < 0 → last perturbation hurt, reverse direction.
         */
        if (dP > 0.0f)
        {
            /* Power increased — continue in the same direction as last step */
            s_duty_pct += (dV >= 0.0f) ? -PERTURB_STEP_PCT : +PERTURB_STEP_PCT;
        }
        else
        {
            /* Power decreased — reverse direction */
            s_duty_pct += (dV >= 0.0f) ? +PERTURB_STEP_PCT : -PERTURB_STEP_PCT;
        }
    }
    /* else: inside dead-zone → already at MPP, make no change */

    /* ── Step 4: Clamp duty cycle to safe operating range ───────────────────*/
    if (s_duty_pct > DUTY_MAX_PCT) s_duty_pct = DUTY_MAX_PCT;
    if (s_duty_pct < DUTY_MIN_PCT) s_duty_pct = DUTY_MIN_PCT;

    /* ── Step 5: Apply new duty cycle to PWM peripheral ────────────────────*/
    output_pwm(s_duty_pct);

    /* ── Step 6: Store state for next iteration ─────────────────────────────*/
    s_P_prev = P;
    s_V_prev = V;

    /* ── Step 7: Update LED status indicator ────────────────────────────────*/
    update_status_led(dP);

    /* ── Step 8: Send telemetry over USART (115200 baud, 8N1) ───────────────*/
    /* Format: "V=xx.xx I=x.xxx P=xx.xxx D=xx.x\r\n"                         */
    sprintf(msg, "V=%.2f\tI=%.3f\tP=%.3f\tD=%.1f\r\n",
            V, I, P, s_duty_pct);
    print_terminal(msg);
}

/* ===========================================================================
 * adc_oversample_voltage()
 *
 * Reads ADC1_CH1 (PA0) ADC_OVERSAMPLE times and returns the mean.
 * Applies the 5:1 voltage divider scale factor to recover V_panel.
 *
 * Returns: V_panel in Volts (0–20V range for 10W panel)
 * ===========================================================================*/
static float adc_oversample_voltage(void)
{
    uint32_t sum = 0;
    for (int i = 0; i < ADC_OVERSAMPLE; i++)
    {
        sum += (uint32_t)convert_PA0;
        delay_nms(1); /* brief settle between reads — reduces correlated noise */
    }
    float raw = (float)sum / (float)ADC_OVERSAMPLE;
    return (raw / ADC_RESOLUTION) * ADC_VREF * V_DIVIDER_RATIO;
}

/* ===========================================================================
 * adc_oversample_current()
 *
 * Reads ADC1_CH2 (PA1) ADC_OVERSAMPLE times and returns the mean.
 * Applies shunt resistance and op-amp gain to recover I_panel.
 *
 * Returns: I_panel in Amperes (0–0.67A range at MPP for 10W/15V panel)
 * ===========================================================================*/
static float adc_oversample_current(void)
{
    uint32_t sum = 0;
    for (int i = 0; i < ADC_OVERSAMPLE; i++)
    {
        sum += (uint32_t)convert_PA1;
        delay_nms(1);
    }
    float raw = (float)sum / (float)ADC_OVERSAMPLE;
    /* V_adc = I_panel × R_shunt × G_opamp  →  I_panel = V_adc / (R × G) */
    return (raw / ADC_RESOLUTION) * ADC_VREF / (I_SHUNT_OHM * I_OPAMP_GAIN);
}

/* ===========================================================================
 * update_status_led()
 *
 * Uses PB3 (onboard LED LD3) to give a visual indication of MPPT state.
 * The Nucleo-32 F303K8 only has one onboard LED; behaviour documented below.
 *
 * Encoding:
 *   |ΔP| ≤ dead-zone → steady ON  (MPP locked, no perturbation needed)
 *   |ΔP| > dead-zone → blink      (tracking in progress)
 *   duty at limit    → rapid blink (fault — check hardware)
 * ===========================================================================*/
static void update_status_led(float dP)
{
    if (fabsf(dP) <= POWER_DEADZONE_W)
    {
        /* MPP locked: LED stays ON */
        led_on();
    }
    else if (s_duty_pct >= DUTY_MAX_PCT || s_duty_pct <= DUTY_MIN_PCT)
    {
        /* Duty rail hit: fast blink to signal potential fault */
        led_on();  delay_nms(50);
        led_off(); delay_nms(50);
        led_on();  delay_nms(50);
        led_off(); delay_nms(50);
    }
    /* else: tracking — LED is already toggled by the led_on/led_off in main() */
}
