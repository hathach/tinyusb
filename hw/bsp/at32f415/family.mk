# Submodules
AT32F415_SDK = hw/mcu/artery/at32f415
DEPS_SUBMODULES += $(AT32F415_SDK)

# AT32 SDK path
AT32F415_SDK_SRC = $(AT32F415_SDK)/libraries

include $(TOP)/$(BOARD_PATH)/board.mk

CPU_CORE ?= cortex-m3

CFLAGS_GCC += \
  -flto

CFLAGS += \
	-DCFG_TUSB_MCU=OPT_MCU_AT32F415 \

LDFLAGS_GCC += \
	-flto --specs=nosys.specs

SRC_C += \
	src/portable/synopsys/dwc2/dcd_dwc2.c \
	$(AT32F415_SDK_SRC)/drivers/src/at32f415_gpio.c \
	$(AT32F415_SDK_SRC)/drivers/src/at32f415_misc.c \
	$(AT32F415_SDK_SRC)/drivers/src/at32f415_usart.c \
	$(AT32F415_SDK_SRC)/drivers/src/at32f415_crm.c \
	$(AT32F415_SDK_SRC)/cmsis/cm4/device_support/system_at32f415.c

INC += \
	$(TOP)/$(BOARD_PATH) \
	$(TOP)/$(AT32F415_SDK_SRC)/drivers/inc \
	$(TOP)/$(AT32F415_SDK_SRC)/cmsis/cm4/core_support \
	$(TOP)/$(AT32F415_SDK_SRC)/cmsis/cm4/device_support

SRC_S += \
	$(FAMILY_PATH)/startup_at32f415.s

# For freeRTOS port source
FREERTOS_PORTABLE_SRC = $(FREERTOS_PORTABLE_PATH)/ARM_CM3

flash: flash-stlink
