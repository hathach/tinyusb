SDK_DIR = hw/mcu/nxp/mcux-sdk
DEPS_SUBMODULES += $(SDK_DIR)

MCU_DIR = $(SDK_DIR)/devices/$(MCU)
include $(TOP)/$(BOARD_PATH)/board.mk

CFLAGS += \
  -mthumb \
  -mabi=aapcs \
	-DCFG_TUSB_MCU=OPT_MCU_KINETIS_K32

SRC_C += \
	src/portable/nxp/khci/dcd_khci.c \
	src/portable/nxp/khci/hcd_khci.c \
	$(MCU_DIR)/system_$(MCU).c \
	$(MCU_DIR)/drivers/fsl_clock.c \
	$(SDK_DIR)/drivers/gpio/fsl_gpio.c \
	$(SDK_DIR)/drivers/lpuart/fsl_lpuart.c

INC += \
	$(TOP)/$(BOARD_PATH) \
	$(TOP)/$(SDK_DIR)/CMSIS/Include \
	$(TOP)/$(MCU_DIR) \
	$(TOP)/$(MCU_DIR)/project_template \
	$(TOP)/$(MCU_DIR)/drivers \
	$(TOP)/$(SDK_DIR)/drivers/smc \
	$(TOP)/$(SDK_DIR)/drivers/common \
	$(TOP)/$(SDK_DIR)/drivers/gpio \
	$(TOP)/$(SDK_DIR)/drivers/port \
	$(TOP)/$(SDK_DIR)/drivers/lpuart \

SRC_S += $(MCU_DIR)/gcc/startup_$(MCU).S
