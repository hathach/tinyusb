SDK_DIR = hw/mcu/nxp/mcux-sdk
DEPS_SUBMODULES += $(SDK_DIR)

CFLAGS += \
  -flto \
  -mthumb \
  -mabi=aapcs \
  -mcpu=cortex-m0plus \
  -DCPU_LPC51U68JBD64 \
  -DCFG_TUSB_MCU=OPT_MCU_LPC51UXX \
  -DCFG_TUSB_MEM_SECTION='__attribute__((section(".data")))' \
  -DCFG_TUSB_MEM_ALIGN='__attribute__((aligned(64)))' 

# mcu driver cause following warnings
CFLAGS += -Wno-error=unused-parameter

MCU_DIR = $(SDK_DIR)/devices/LPC51U68

# All source paths should be relative to the top level.
LD_FILE = $(MCU_DIR)/gcc/LPC51U68_flash.ld

SRC_C += \
	src/portable/nxp/lpc_ip3511/dcd_lpc_ip3511.c \
	$(MCU_DIR)/system_LPC51U68.c \
	$(MCU_DIR)/drivers/fsl_clock.c \
	$(MCU_DIR)/drivers/fsl_power.c \
	$(MCU_DIR)/drivers/fsl_reset.c \
	$(SDK_DIR)/drivers/lpc_gpio/fsl_gpio.c \
	$(SDK_DIR)/drivers/flexcomm/fsl_flexcomm.c \
	$(SDK_DIR)/drivers/flexcomm/fsl_usart.c

INC += \
	$(TOP)/$(MCU_DIR)/../../CMSIS/Include \
	$(TOP)/$(MCU_DIR) \
	$(TOP)/$(MCU_DIR)/drivers \
	$(TOP)/$(SDK_DIR)/drivers/common \
	$(TOP)/$(SDK_DIR)/drivers/flexcomm \
	$(TOP)/$(SDK_DIR)/drivers/lpc_iocon \
	$(TOP)/$(SDK_DIR)/drivers/lpc_gpio	

SRC_S += $(MCU_DIR)/gcc/startup_LPC51U68.S

LIBS += $(TOP)/$(MCU_DIR)/gcc/libpower.a

# For freeRTOS port source
FREERTOS_PORT = ARM_CM0

JLINK_DEVICE = LPC51U68
PYOCD_TARGET = LPC51U68

# flash using pyocd (51u68 is not supported yet)
flash: flash-pyocd
