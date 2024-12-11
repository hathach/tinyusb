# Submodules
AT32F403A_407_SDK = hw/mcu/artery/at32f403a_407
DEPS_SUBMODULES += $(AT32F403A_407_SDK)

# AT32 SDK path
AT32F403A_407_SDK_SRC = $(AT32F403A_407_SDK)/libraries

include $(TOP)/$(BOARD_PATH)/board.mk

CPU_CORE ?= cortex-m4

CFLAGS_GCC += \
  -flto

CFLAGS += \
	-DCFG_TUSB_MCU=OPT_MCU_AT32F403A_407

LDFLAGS_GCC += \
	-flto --specs=nosys.specs

SRC_C += \
	src/portable/st/stm32_fsdev/dcd_stm32_fsdev.c \
	$(AT32F403A_407_SDK_SRC)/drivers/src/at32f403a_407_gpio.c \
	$(AT32F403A_407_SDK_SRC)/drivers/src/at32f403a_407_misc.c \
	$(AT32F403A_407_SDK_SRC)/drivers/src/at32f403a_407_usart.c \
	$(AT32F403A_407_SDK_SRC)/drivers/src/at32f403a_407_acc.c \
	$(AT32F403A_407_SDK_SRC)/drivers/src/at32f403a_407_crm.c \
	$(AT32F403A_407_SDK_SRC)/cmsis/cm4/device_support/system_at32f403a_407.c

INC += \
	$(TOP)/$(BOARD_PATH) \
	$(TOP)/$(AT32F403A_407_SDK_SRC)/drivers/inc \
	$(TOP)/$(AT32F403A_407_SDK_SRC)/cmsis/cm4/core_support \
	$(TOP)/$(AT32F403A_407_SDK_SRC)/cmsis/cm4/device_support

SRC_S += \
	$(FAMILY_PATH)/startup_at32f403a_407.s

# For freeRTOS port source
FREERTOS_PORTABLE_SRC = $(FREERTOS_PORTABLE_PATH)/ARM_CM4F

flash: flash-stlink
