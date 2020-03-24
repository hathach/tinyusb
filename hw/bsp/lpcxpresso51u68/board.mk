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

MCU_DIR = hw/mcu/nxp/sdk/devices/LPC51U68

# All source paths should be relative to the top level.
LD_FILE = $(MCU_DIR)/gcc/LPC51U68_flash.ld

SRC_C += \
	$(MCU_DIR)/system_LPC51U68.c \
	$(MCU_DIR)/drivers/fsl_clock.c \
	$(MCU_DIR)/drivers/fsl_gpio.c \
	$(MCU_DIR)/drivers/fsl_power.c \
	$(MCU_DIR)/drivers/fsl_reset.c

INC += \
	$(TOP)/$(MCU_DIR)/../../CMSIS/Include \
	$(TOP)/$(MCU_DIR) \
	$(TOP)/$(MCU_DIR)/drivers

SRC_S += $(MCU_DIR)/gcc/startup_LPC51U68.S

LIBS += $(TOP)/$(MCU_DIR)/gcc/libpower.a

# For TinyUSB port source
VENDOR = nxp
CHIP_FAMILY = lpc_ip3511

# For freeRTOS port source
FREERTOS_PORT = ARM_CM0

# For flash-jlink target
JLINK_DEVICE = LPC51U68
JLINK_IF = swd

# flash using pyocd (51u68 is not supported yet)
flash: $(BUILD)/$(BOARD)-firmware.hex
	pyocd flash -t LPC51U68 $<
