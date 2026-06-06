##############################################################################
# Makefile — arm-mppt-boost-converter
# STM32F303K8T Nucleo-32 | arm-none-eabi-gcc | PUSL3198
##############################################################################
#
# PREREQUISITES (install once, then `make` from this directory):
#
#   Linux / WSL:
#       sudo apt install gcc-arm-none-eabi binutils-arm-none-eabi
#
#   macOS (Homebrew):
#       brew install --cask gcc-arm-embedded
#
#   Windows:
#       Download Arm GNU Toolchain from developer.arm.com
#
# CMSIS DEVICE HEADERS (required for compilation):
#   The STM32F3xx device headers are NOT distributed here (ST licence).
#   Download the CMSIS pack from either:
#     A) STM32CubeIDE — opens automatically when building in the IDE
#     B) Manual:  git clone https://github.com/STMicroelectronics/cmsis-device-f3
#                 Then set CMSIS_DEVICE_INC below to the /Include path
#     C) Keil MDK — open the .uvprojx in /keil_project/
#
# QUICK START with STM32CubeIDE (recommended):
#   File → Import → Existing Projects → browse to this folder
#   Right-click project → Properties → C/C++ Build → check device is STM32F303K8Tx
#   Build → Flash with ST-Link on Nucleo-32
#
##############################################################################

TARGET    := mppt_boost
BUILD_DIR := build
DEVICE    := STM32F303x8

##############################################################################
# Toolchain
##############################################################################
CC      := arm-none-eabi-gcc
AS      := arm-none-eabi-gcc -x assembler-with-cpp
OBJCOPY := arm-none-eabi-objcopy
SIZE    := arm-none-eabi-size

##############################################################################
# Paths — set CMSIS_CORE_INC and CMSIS_DEVICE_INC for your installation
##############################################################################

# Default: assumes cmsis-device-f3 cloned alongside this repo.
# Override from command line:  make CMSIS_DEVICE_INC=/path/to/Include
CMSIS_CORE_INC   ?= ../cmsis_core/CMSIS/Core/Include
CMSIS_DEVICE_INC ?= ../cmsis-device-f3/Include

INC_DIRS := \
    firmware \
    $(CMSIS_CORE_INC) \
    $(CMSIS_DEVICE_INC)

INCLUDES := $(addprefix -I,$(INC_DIRS))

##############################################################################
# Source files
##############################################################################
C_SOURCES := \
    firmware/main.c         \
    firmware/adc.c          \
    firmware/pwm.c          \
    firmware/leds.c         \
    firmware/usart.c        \
    firmware/systick_delay.c \
    firmware/PLL_Config.c   \
    firmware/device/system_stm32f3xx.c

ASM_SOURCES := \
    firmware/device/startup_stm32f303x8.s

##############################################################################
# Compiler flags
##############################################################################

# Cortex-M4 with FPU (STM32F303 uses Cortex-M4F)
CPU  := -mcpu=cortex-m4
FPU  := -mfpu=fpv4-sp-d16
FLOAT_ABI := -mfloat-abi=hard
MCU  := $(CPU) -mthumb $(FPU) $(FLOAT_ABI)

CFLAGS := $(MCU) \
    -D$(DEVICE) \
    -DUSE_FULL_LL_DRIVER \
    $(INCLUDES) \
    -Wall \
    -Wextra \
    -fdata-sections \
    -ffunction-sections \
    -O2 \
    -g3

ASFLAGS := $(MCU) \
    -D$(DEVICE) \
    $(INCLUDES) \
    -Wall \
    -fdata-sections \
    -ffunction-sections

##############################################################################
# Linker flags
##############################################################################

# STM32F303K8: 64KB Flash at 0x08000000, 12KB RAM at 0x20000000
LDSCRIPT := firmware/device/STM32F303K8Tx_FLASH.ld

LDFLAGS := $(MCU) \
    -specs=nano.specs \
    -T$(LDSCRIPT) \
    -Wl,-Map=$(BUILD_DIR)/$(TARGET).map,--cref \
    -Wl,--gc-sections \
    -lm

##############################################################################
# Object files
##############################################################################
OBJECTS := $(addprefix $(BUILD_DIR)/,$(notdir $(C_SOURCES:.c=.o)))
OBJECTS += $(addprefix $(BUILD_DIR)/,$(notdir $(ASM_SOURCES:.s=.o)))

vpath %.c $(sort $(dir $(C_SOURCES)))
vpath %.s $(sort $(dir $(ASM_SOURCES)))

##############################################################################
# Build rules
##############################################################################
.PHONY: all clean flash info

all: $(BUILD_DIR)/$(TARGET).elf $(BUILD_DIR)/$(TARGET).hex $(BUILD_DIR)/$(TARGET).bin

$(BUILD_DIR)/%.o: %.c Makefile | $(BUILD_DIR)
	$(CC) -c $(CFLAGS) -Wa,-a,-ad,-alms=$(BUILD_DIR)/$(notdir $(<:.c=.lst)) $< -o $@

$(BUILD_DIR)/%.o: %.s Makefile | $(BUILD_DIR)
	$(AS) -c $(ASFLAGS) $< -o $@

$(BUILD_DIR)/$(TARGET).elf: $(OBJECTS) Makefile
	$(CC) $(OBJECTS) $(LDFLAGS) -o $@
	$(SIZE) $@

$(BUILD_DIR)/%.hex: $(BUILD_DIR)/%.elf
	$(OBJCOPY) -O ihex $< $@

$(BUILD_DIR)/%.bin: $(BUILD_DIR)/%.elf
	$(OBJCOPY) -O binary -S $< $@

$(BUILD_DIR):
	mkdir -p $@

##############################################################################
# Flash with OpenOCD (ST-Link on Nucleo board)
##############################################################################
flash: $(BUILD_DIR)/$(TARGET).bin
	openocd -f interface/stlink.cfg \
	        -f target/stm32f3x.cfg \
	        -c "program $(BUILD_DIR)/$(TARGET).bin verify reset exit 0x08000000"

##############################################################################
# Info
##############################################################################
info:
	@echo "Target:   $(TARGET)"
	@echo "Device:   STM32F303K8T (Cortex-M4F, 64KB Flash, 12KB RAM)"
	@echo "Toolchain: arm-none-eabi-gcc"
	@echo "PWM:      TIM3 CH3 → PB0 @ 100 kHz"
	@echo "ADC:      PA0 (voltage), PA1 (current) @ ADC1"

clean:
	rm -rf $(BUILD_DIR)

##############################################################################
# Dependency tracking
##############################################################################
-include $(wildcard $(BUILD_DIR)/*.d)
