CFLAGS += \
  -mthumb \
  -mabi=aapcs \
  -mcpu=cortex-m0plus \
  -DCORE_M0PLUS \
  -DCFG_TUSB_MCU=OPT_MCU_LPC51UXX \
  -DCPU_LPC51U68JBD64 \
  -Wfatal-errors \
  -DCFG_TUSB_MEM_SECTION='__attribute__((section(".data")))' \
  -DCFG_TUSB_MEM_ALIGN='__attribute__((aligned(64)))' 

MCU_DIR = hw/mcu/nxp/lpc_driver/lpc51u6x/devices/LPC51U68

# All source paths should be relative to the top level.
LD_FILE = $(MCU_DIR)/gcc/LPC51U68_flash.ld

SRC_C += \
	$(MCU_DIR)/system_LPC51U68.c \
	$(MCU_DIR)/drivers/fsl_clock.c \
	$(MCU_DIR)/drivers/fsl_gpio.c \
	$(MCU_DIR)/drivers/fsl_power.c \
	$(MCU_DIR)/drivers/fsl_reset.c

INC += \
	$(TOP)/hw/mcu/nxp/lpc_driver/lpc51u6x/CMSIS/Include \
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

# flash using jlink
#flash: flash-jlink
