CFLAGS += \
  -mthumb \
  -mabi=aapcs \
  -mcpu=cortex-m0plus \
  -DCORE_M0PLUS \
  -DCFG_TUSB_MCU=OPT_MCU_LPC51UXX \
  -DCPU_LPC51U68JBD64 \
  -Wfatal-errors \
  -DCFG_TUSB_MEM_SECTION='__attribute__((section(".data.$$RAM3")))' \
  -DCFG_TUSB_MEM_ALIGN='__attribute__((aligned(64)))' 

# All source paths should be relative to the top level.
LD_FILE = hw/bsp/lpcxpresso51u68/LPC51U68_flash.ld

SRC_C += \
	hw/mcu/nxp/lpcopen/lpc51u6x/system_LPC51U68.c \
	hw/mcu/nxp/lpcopen/lpc51u6x/drivers/fsl_clock.c \
	hw/mcu/nxp/lpcopen/lpc51u6x/drivers/fsl_gpio.c

INC += \
	$(TOP)/hw/mcu/nxp/lpcopen/lpc51u6x \
	$(TOP)/hw/mcu/nxp/lpcopen/lpc51u6x/CMSIS/Include \
	$(TOP)/hw/mcu/nxp/lpcopen/lpc51u6x/drivers

SRC_S += hw/bsp/lpcxpresso51u68/startup_LPC51U68.S

# For TinyUSB port source
VENDOR = nxp
CHIP_FAMILY = lpc11_13_15

# For freeRTOS port source
FREERTOS_PORT = ARM_CM0

# For flash-jlink target
JLINK_DEVICE = LPC51U68
JLINK_IF = swd

# flash using jlink
flash: flash-jlink
