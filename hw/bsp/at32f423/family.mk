# Submodules
AT32F423_SDK = hw/mcu/artery/at32f423
DEPS_SUBMODULES += $(AT32F423_SDK)

# AT32 SDK path
AT32F423_SDK_SRC = $(AT32F423_SDK)/libraries

include $(TOP)/$(BOARD_PATH)/board.mk

CPU_CORE ?= cortex-m4

CFLAGS_GCC += \
  -flto

CFLAGS += \
	-DCFG_TUSB_MCU=OPT_MCU_AT32F423 \

LDFLAGS_GCC += \
	-flto --specs=nosys.specs

SRC_C += \
	src/portable/synopsys/dwc2/dcd_dwc2.c \
	src/portable/synopsys/dwc2/hcd_dwc2.c \
	src/portable/synopsys/dwc2/dwc2_common.c \
	$(AT32F423_SDK_SRC)/drivers/src/at32f423_gpio.c \
	$(AT32F423_SDK_SRC)/drivers/src/at32f423_misc.c \
	$(AT32F423_SDK_SRC)/drivers/src/at32f423_usart.c \
	$(AT32F423_SDK_SRC)/drivers/src/at32f423_crm.c \
	$(AT32F423_SDK_SRC)/drivers/src/at32f423_acc.c \
	$(AT32F423_SDK_SRC)/cmsis/cm4/device_support/system_at32f423.c

INC += \
	$(TOP)/$(BOARD_PATH) \
	$(TOP)/$(AT32F423_SDK_SRC)/drivers/inc \
	$(TOP)/$(AT32F423_SDK_SRC)/cmsis/cm4/core_support \
	$(TOP)/$(AT32F423_SDK_SRC)/cmsis/cm4/device_support

SRC_S += \
	$(FAMILY_PATH)/startup_at32f423.s

# For freeRTOS port source
#FREERTOS_PORTABLE_SRC = $(FREERTOS_PORTABLE_PATH)/ARM_CM4

flash: flash-stlink
