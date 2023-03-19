MCU = LPC51U68

CFLAGS += \
  -DCPU_LPC51U68JBD64 \
  -DCFG_TUSB_MEM_SECTION='__attribute__((section(".data")))'

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
FREERTOS_PORTABLE_SRC = $(FREERTOS_PORTABLE_PATH)/ARM_CM0

JLINK_DEVICE = LPC51U68
PYOCD_TARGET = LPC51U68

# flash using pyocd (51u68 is not supported yet)
flash: flash-pyocd
