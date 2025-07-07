# Submodules
AT32F402_405_SDK = hw/mcu/artery/at32f402_405
DEPS_SUBMODULES += $(AT32F402_405_SDK)

# AT32 SDK path
AT32F402_405_SDK_SRC = $(AT32F402_405_SDK)/libraries

include $(TOP)/$(BOARD_PATH)/board.mk

CPU_CORE ?= cortex-m4

CFLAGS_GCC += \
  -flto

CFLAGS += \
	-DCFG_TUSB_MCU=OPT_MCU_AT32F402_405 \
	-DBOARD_TUD_MAX_SPEED=OPT_MODE_HIGH_SPEED

LDFLAGS_GCC += \
	-flto --specs=nosys.specs

SRC_C += \
	src/portable/synopsys/dwc2/dcd_dwc2.c \
	src/portable/synopsys/dwc2/hcd_dwc2.c \
	src/portable/synopsys/dwc2/dwc2_common.c \
	$(AT32F402_405_SDK_SRC)/drivers/src/at32f402_405_gpio.c \
	$(AT32F402_405_SDK_SRC)/drivers/src/at32f402_405_misc.c \
	$(AT32F402_405_SDK_SRC)/drivers/src/at32f402_405_usart.c \
	$(AT32F402_405_SDK_SRC)/drivers/src/at32f402_405_crm.c \
	$(AT32F402_405_SDK_SRC)/drivers/src/at32f402_405_acc.c \
	$(AT32F402_405_SDK_SRC)/cmsis/cm4/device_support/system_at32f402_405.c

INC += \
	$(TOP)/$(BOARD_PATH) \
	$(TOP)/$(AT32F402_405_SDK_SRC)/drivers/inc \
	$(TOP)/$(AT32F402_405_SDK_SRC)/cmsis/cm4/core_support \
	$(TOP)/$(AT32F402_405_SDK_SRC)/cmsis/cm4/device_support

SRC_S += \
	$(FAMILY_PATH)/startup_at32f402_405.s

# For freeRTOS port source
#FREERTOS_PORTABLE_SRC = $(FREERTOS_PORTABLE_PATH)/ARM_CM4

flash: flash-atlink
