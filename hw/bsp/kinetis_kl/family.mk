SDK_DIR = hw/mcu/nxp/nxp_sdk
DEPS_SUBMODULES += $(SDK_DIR)

MCU_DIR = $(SDK_DIR)/devices/$(MCU)
include $(TOP)/$(BOARD_PATH)/board.mk

CFLAGS += \
  -mthumb \
  -mabi=aapcs \
  -mcpu=cortex-m0plus \
  -DCFG_TUSB_MCU=OPT_MCU_KINETIS_KL \

LDFLAGS += \
  -Wl,--defsym,__stack_size__=0x400 \
  -Wl,--defsym,__heap_size__=0

SRC_C += \
	src/portable/nxp/khci/dcd_khci.c \
	src/portable/nxp/khci/hcd_khci.c \
	$(MCU_DIR)/system_$(MCU).c \
	$(MCU_DIR)/project_template/clock_config.c \
	$(MCU_DIR)/drivers/fsl_clock.c \
	$(MCU_DIR)/drivers/fsl_gpio.c \
	$(MCU_DIR)/drivers/fsl_lpsci.c \
	$(MCU_DIR)/drivers/fsl_uart.c

INC += \
	$(TOP)/$(BOARD_PATH) \
	$(TOP)/$(SDK_DIR)/CMSIS/Include \
	$(TOP)/$(MCU_DIR) \
	$(TOP)/$(MCU_DIR)/project_template \
	$(TOP)/$(MCU_DIR)/drivers

SRC_S += $(MCU_DIR)/gcc/startup_$(MCU).S

# For freeRTOS port source
FREERTOS_PORTABLE_SRC = $(FREERTOS_PORTABLE_PATH)/ARM_CM0
