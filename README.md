# MPPT Boost Converter Firmware
### Design and Control of Renewable Energy Technology
**STM32F303K8T (Nucleo-32) | 100 kHz PWM | Perturb & Observe MPPT**

---


### Technical Badges
![STM32](https://img.shields.io/badge/MCU-STM32F303K8-blue?style=for-the-badge&logo=stmicroelectronics)
![C](https://img.shields.io/badge/Language-C-green?style=for-the-badge&logo=c)
![Frequency](https://img.shields.io/badge/Switching_Freq-100kHz-orange?style=for-the-badge)
![Domain](https://img.shields.io/badge/Domain-Power_Electronics-red?style=for-the-badge)

---

## System Overview

This firmware implements a Maximum Power Point Tracking (MPPT) controller for a 10W photovoltaic panel using a DC-DC boost converter. It runs on the STM32F303K8T Nucleo-32 development board and controls an IRF510 MOSFET via a TC4429 isolated gate driver at 100 kHz.

**Theoretical converter efficiency: 94.65%** at rated MPP (10W, 15V in → 30V out, D=0.5).

---

## Hardware Architecture

```
PV Panel (10W, 20V)
    │
    ├── [40kΩ/10kΩ divider] ──[MCP602 buffer]──[RC LPF 390Ω/10nF]──[4.7V Zener]──► PA0 (V_SENSE)
    │
    └── [1Ω shunt] ──────────[MCP602 buffer]──[RC LPF 390Ω/10nF]──[4.7V Zener]──► PA1 (I_SENSE)

STM32F303K8T (Nucleo-32 @ 64 MHz)
    │
    └── PB0 (TIM3_CH3 PWM 100kHz) ──[56Ω R_Gate]──► TC4429 Gate Driver (12V)
                                                              │
                                                         IRF510 MOSFET
                                                              │
                                                    Boost Converter Circuit
                                                    L: 3C90 Ferrite Toroid
                                                    D: 1N4002
                                                    C: 2× output caps (parallel)
                                                    R_Load: 100Ω / 5W
```

| Signal | Pin | Function |
|--------|-----|----------|
| V_SENSE | PA0 | ADC1_CH1 — panel voltage via 5:1 divider |
| I_SENSE | PA1 | ADC1_CH2 — panel current via 1Ω shunt |
| PWM_OUT | PB0 | TIM3_CH3 — 100 kHz gate drive signal |
| LED | PB3 | Onboard LD3 — MPPT status indicator |
| UART TX | PA2 | USART2 — telemetry at 115200 baud |

---

## Repository Structure

```
arm-mppt-boost-converter/
├── firmware/
│   ├── main.c              ← Core P&O control loop + state machine
│   ├── adc.c / adc.h       ← ADC1 register config + 8× oversampling
│   ├── pwm.c / pwm.h       ← TIM3 CH3 100 kHz PWM peripheral setup
│   ├── leds.c / leds.h     ← PB3 LED indicator driver
│   ├── usart.c / usart.h   ← USART2 telemetry (115200 baud)
│   ├── systick_delay.c/h   ← 1 ms SysTick delay
│   ├── PLL_Config.c/h      ← HSI × 16 = 64 MHz PLL setup
│   ├── PWR.h               ← Sleep mode macros
│   ├── main.h              ← System constants (Fcy, priorities)
│   └── device/
│       ├── startup_stm32f303x8.s   ← ARM reset handler + vector table
│       ├── system_stm32f3xx.c      ← CMSIS system init
│       └── STM32F303K8Tx_FLASH.ld  ← Linker script (64KB Flash, 12KB RAM)
├── Makefile                ← arm-none-eabi-gcc build system
├── MPPT_Report.pdf
└── README.md
```

---

## P&O Algorithm

The Perturb & Observe algorithm adjusts the boost converter duty cycle by ±0.5% every 100 ms and observes the resulting change in output power:

```
Measure V(k), I(k)
P(k) = V(k) × I(k)
ΔP   = P(k) − P(k−1)
ΔV   = V(k) − V(k−1)

if |ΔP| > 5 mW:           ← outside dead-zone: still tracking
    if ΔP > 0:
        duty += (ΔV ≥ 0) ? −0.5% : +0.5%   ← continue same direction
    else:
        duty += (ΔV ≥ 0) ? +0.5% : −0.5%   ← reverse direction
else:                      ← inside dead-zone: MPP locked, hold duty
    (no change)

duty = clamp(duty, 10%, 90%)
output_pwm(duty)
```

**Key parameters:**

| Parameter | Value | Rationale |
|-----------|-------|-----------|
| Perturbation step | 0.5% | Fine enough to find MPP, fast enough to track |
| Dead-zone | 5 mW | Stops oscillation once settled at MPP |
| Loop period | 100 ms | Allows output capacitors to fully settle |
| Duty limits | 10–90% | Prevents inductor saturation at low duty; OV at high duty |
| ADC oversampling | ×8 | Reduces noise by ~2.8×; effective resolution ≈13.5 bits |

---

## Power Loss Budget (at 10W MPP, D=0.5, 100 kHz)

| Component | Loss (W) | % of Input | Source |
|-----------|----------|------------|--------|
| 1N4002 diode conduction | 0.335 | 3.35% | Vf = 1.0V |
| IRF510 MOSFET switching | 0.029 | 0.29% | tr=16ns, tf=9.4ns |
| IRF510 MOSFET conduction | 0.121 | 1.21% | R_DS(on)=0.54Ω |
| 3C90 core losses | 0.050 | 0.50% | Datasheet @ 100kHz |
| **Total** | **0.535** | **5.35%** | |
| **Efficiency η** | — | **94.65%** | |

> **Improvement opportunity:** replacing the 1N4002 with a Schottky diode (e.g. 1N5819, Vf≈0.3V) would reduce diode conduction loss by ~70%, pushing efficiency above 97%.

---

## Build Instructions

### Option 1: STM32CubeIDE (recommended)

1. Install [STM32CubeIDE](https://www.st.com/en/development-tools/stm32cubeide.html)
2. **File → Import → General → Existing Projects into Workspace**
3. Select this folder — CubeIDE will recognise the Makefile project
4. Right-click project → **Properties → MCU/MPU GCC Compiler** → verify `STM32F303K8Tx`
5. **Project → Build** then **Run → Debug** (flash via ST-Link on Nucleo USB connector)

### Option 2: Keil MDK

Open `keil_project/STM_code.uvprojx` — the original Keil project is included unchanged.

### Option 3: arm-none-eabi-gcc (CLI)

```bash
# Install toolchain (Ubuntu/Debian/WSL)
sudo apt install gcc-arm-none-eabi binutils-arm-none-eabi

# Get STM32F3 CMSIS device headers (required, Apache 2.0 licence)
git clone https://github.com/STMicroelectronics/cmsis-device-f3 ../cmsis-device-f3
git clone https://github.com/ARM-software/CMSIS_5 ../cmsis_core

# Build
make CMSIS_CORE_INC=../cmsis_core/CMSIS/Core/Include \
     CMSIS_DEVICE_INC=../cmsis-device-f3/Include

# Flash with OpenOCD + ST-Link
make flash
```

---

## Telemetry Output

Connect a serial terminal (PuTTY, screen, minicom) to the Nucleo virtual COM port:
- **Baud rate:** 115200
- **Format:** 8N1

Output format (tab-separated):
```
V=15.23   I=0.657   P=10.006   D=50.5
V=15.20   I=0.659   P=10.027   D=50.0
V=15.21   I=0.659   P=10.033   D=50.0   ← MPP locked (ΔP < 5mW dead-zone)
```

**LED status (PB3, onboard LD3):**
- Steady ON: MPP locked
- Slow blink (100ms): actively tracking
- Fast blink (50ms): duty cycle limit reached — check circuit

---

## Algorithm Comparison (Chapter 4 summary)

| | P&O (implemented) | INC | FOCV |
|--|--|--|--|
| Complexity | Low | High | Very Low |
| Steady-state oscillation | Yes, ±0.5% | No | No |
| Fast irradiance tracking |  Can fail |  Correct |  Slow |
| Sensor requirements | V + I | V + I | V only |
| Recommended for | Stable conditions | Cloudy/variable | Cost-sensitive |

---




*W.L.P.T.N. Wijayasinghe 
