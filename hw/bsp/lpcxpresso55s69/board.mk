CFLAGS += \
  -mthumb \
  -mabi=aapcs \
  -mcpu=cortex-m33 \
  -mfloat-abi=hard \
  -mfpu=fpv5-sp-d16 \
  -DCPU_LPC55S69JBD100_cm33_core0 \
  -DCFG_TUSB_MCU=OPT_MCU_LPC55XX \
  -DCFG_TUSB_MEM_SECTION='__attribute__((section(".data")))' \
  -DCFG_TUSB_MEM_ALIGN='__attribute__((aligned(64)))'

# mcu driver cause following warnings
CFLAGS += -Wno-error=unused-parameter -Wno-error=float-equal -Wno-error=nested-externs

MCU_DIR = hw/mcu/nxp/sdk/devices/LPC55S69

# All source paths should be relative to the top level.
LD_FILE = $(MCU_DIR)/gcc/LPC55S69_cm33_core0_flash.ld

SRC_C += \
	$(MCU_DIR)/system_LPC55S69_cm33_core0.c \
	$(MCU_DIR)/drivers/fsl_clock.c \
	$(MCU_DIR)/drivers/fsl_gpio.c \
	$(MCU_DIR)/drivers/fsl_power.c \
	$(MCU_DIR)/drivers/fsl_reset.c

INC += \
	$(TOP)/$(MCU_DIR)/../../CMSIS/Include \
	$(TOP)/$(MCU_DIR) \
	$(TOP)/$(MCU_DIR)/drivers

SRC_S += $(MCU_DIR)/gcc/startup_LPC55S69_cm33_core0.S

LIBS += $(TOP)/$(MCU_DIR)/gcc/libpower_hardabi.a

# For TinyUSB port source
VENDOR = nxp
CHIP_FAMILY = lpc_ip3511

# For freeRTOS port source
FREERTOS_PORT = ARM_CM33

# For flash-jlink target
JLINK_DEVICE = LPC55S69
JLINK_IF = swd

# flash using pyocd
flash: $(BUILD)/$(BOARD)-firmware.hex
	pyocd flash -t LPC55S69 $<
