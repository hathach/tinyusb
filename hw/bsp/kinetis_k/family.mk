SDK_DIR = hw/mcu/nxp/mcux-sdk
DEPS_SUBMODULES += $(SDK_DIR) lib/CMSIS_5

MCU_DIR = $(SDK_DIR)/devices/${MCU_VARIANT}
include $(TOP)/$(BOARD_PATH)/board.mk
CPU_CORE ?= cortex-m4

CFLAGS += \
  -D__STARTUP_CLEAR_BSS \
  -DCFG_TUSB_MCU=OPT_MCU_KINETIS_K \

LDFLAGS += \
  -nostartfiles \
  --specs=nosys.specs --specs=nano.specs \
  -Wl,--defsym,__stack_size__=0x400 \
  -Wl,--defsym,__heap_size__=0

SRC_C += \
	src/portable/nxp/khci/dcd_khci.c \
	src/portable/nxp/khci/hcd_khci.c \
	$(MCU_DIR)/system_${MCU_VARIANT}.c \
	$(MCU_DIR)/drivers/fsl_clock.c \
	$(SDK_DIR)/drivers/gpio/fsl_gpio.c \
	$(SDK_DIR)/drivers/uart/fsl_uart.c \

INC += \
	$(TOP)/$(BOARD_PATH) \
	$(TOP)/lib/CMSIS_5/CMSIS/Core/Include \
	$(TOP)/$(MCU_DIR) \
	$(TOP)/$(MCU_DIR)/drivers \
	$(TOP)/$(SDK_DIR)/drivers/common \
	$(TOP)/$(SDK_DIR)/drivers/gpio \
	$(TOP)/$(SDK_DIR)/drivers/port \
	$(TOP)/$(SDK_DIR)/drivers/smc \
	$(TOP)/$(SDK_DIR)/drivers/sysmpu \
	$(TOP)/$(SDK_DIR)/drivers/uart \

SRC_S += ${SDK_DIR}/devices/${MCU_VARIANT}/gcc/startup_${MCU_VARIANT}.S
