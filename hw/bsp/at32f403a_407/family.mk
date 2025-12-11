AT32_FAMILY = at32f403a_407
AT32_SDK_LIB = hw/mcu/artery/${AT32_FAMILY}/libraries

include $(TOP)/$(BOARD_PATH)/board.mk

CPU_CORE ?= cortex-m4

CFLAGS_GCC += \
  -flto

CFLAGS += \
	-DCFG_TUSB_MCU=OPT_MCU_AT32F403A_407

LDFLAGS_GCC += \
	-flto --specs=nosys.specs -nostdlib -nostartfiles

SRC_C += \
	src/portable/st/stm32_fsdev/dcd_stm32_fsdev.c \
	$(AT32_SDK_LIB)/drivers/src/${AT32_FAMILY}_gpio.c \
	$(AT32_SDK_LIB)/drivers/src/${AT32_FAMILY}_misc.c \
	$(AT32_SDK_LIB)/drivers/src/${AT32_FAMILY}_usart.c \
	$(AT32_SDK_LIB)/drivers/src/${AT32_FAMILY}_acc.c \
	$(AT32_SDK_LIB)/drivers/src/${AT32_FAMILY}_crm.c \
	$(AT32_SDK_LIB)/cmsis/cm4/device_support/system_${AT32_FAMILY}.c

INC += \
	$(TOP)/$(BOARD_PATH) \
	$(TOP)/$(AT32_SDK_LIB)/drivers/inc \
	$(TOP)/$(AT32_SDK_LIB)/cmsis/cm4/core_support \
	$(TOP)/$(AT32_SDK_LIB)/cmsis/cm4/device_support

SRC_S_GCC += ${AT32_SDK_LIB}/cmsis/cm4/device_support/startup/gcc/startup_${AT32_FAMILY}.s
SRC_S_IAR += ${AT32_SDK_LIB}/cmsis/cm4/device_support/startup/iar/startup_${AT32_FAMILY}.s

LD_FILE_GCC ?= ${AT32_SDK_LIB}/cmsis/cm4/device_support/startup/gcc/linker/${MCU_LINKER_NAME}_FLASH.ld
LD_FILE_IAR ?= ${AT32_SDK_LIB}/cmsis/cm4/device_support/startup/iar/linker/${MCU_LINKER_NAME}.icf

# For freeRTOS port source
FREERTOS_PORTABLE_SRC = $(FREERTOS_PORTABLE_PATH)/ARM_CM4F

flash: flash-atlink
