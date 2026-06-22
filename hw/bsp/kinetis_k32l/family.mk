UF2_FAMILY_ID = 0x7f83e793

include $(TOP)/$(BOARD_PATH)/board.mk
CPU_CORE ?= cortex-m0plus
MCUX_CORE = hw/mcu/nxp/mcuxsdk-core
MCUX_DEVICES = hw/mcu/nxp/mcux-devices-kinetis

CFLAGS += \
	-DCFG_TUSB_MCU=OPT_MCU_KINETIS_K32L

LDFLAGS += \
  -nostartfiles \
  -specs=nosys.specs -specs=nano.specs

SRC_C += \
	src/portable/nxp/khci/dcd_khci.c \
	src/portable/nxp/khci/hcd_khci.c \
	$(MCUX_DEVICES)/K32L/$(MCU_VARIANT)/system_$(MCU_VARIANT).c \
	$(MCUX_DEVICES)/K32L/$(MCU_VARIANT)/drivers/fsl_clock.c \
	$(MCUX_CORE)/drivers/gpio/fsl_gpio.c \
	$(MCUX_CORE)/drivers/lpuart/fsl_lpuart.c \
	$(MCUX_CORE)/drivers/common/fsl_common_arm.c

INC += \
	$(TOP)/$(BOARD_PATH) \
	$(TOP)/lib/CMSIS_6/CMSIS/Core/Include \
	$(TOP)/$(MCUX_DEVICES)/K32L/$(MCU_VARIANT) \
	$(TOP)/$(MCUX_DEVICES)/K32L/$(MCU_VARIANT)/drivers \
	$(TOP)/$(MCUX_CORE)/drivers/common \
	$(TOP)/$(MCUX_CORE)/drivers/gpio \
	$(TOP)/$(MCUX_CORE)/drivers/lpuart \
	$(TOP)/$(MCUX_CORE)/drivers/port \
	$(TOP)/$(MCUX_CORE)/drivers/smc

SRC_S += $(MCUX_DEVICES)/K32L/$(MCU_VARIANT)/gcc/startup_$(MCU_VARIANT).S
