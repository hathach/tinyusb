AT32_FAMILY = at32f435_437
AT32_SDK_LIB = hw/mcu/artery/${AT32_FAMILY}/libraries

include $(TOP)/$(BOARD_PATH)/board.mk

CPU_CORE ?= cortex-m4

CFLAGS_GCC += \
  -flto

CFLAGS += \
	-DCFG_TUSB_MCU=OPT_MCU_AT32F435_437 \
	-DBOARD_TUD_RHPORT=1 \
	-DBOARD_TUH_RHPORT=0 \
	-DBOARD_TUD_MAX_SPEED=OPT_MODE_FULL_SPEED \
	-DBOARD_TUH_MAX_SPEED=OPT_MODE_FULL_SPEED \

LDFLAGS_GCC += \
	-flto --specs=nosys.specs -nostdlib -nostartfiles

SRC_C += \
	src/portable/synopsys/dwc2/dcd_dwc2.c \
	src/portable/synopsys/dwc2/hcd_dwc2.c \
	src/portable/synopsys/dwc2/dwc2_common.c \
	$(AT32_SDK_LIB)/drivers/src/${AT32_FAMILY}_gpio.c \
	$(AT32_SDK_LIB)/drivers/src/${AT32_FAMILY}_misc.c \
	$(AT32_SDK_LIB)/drivers/src/${AT32_FAMILY}_usart.c \
	$(AT32_SDK_LIB)/drivers/src/${AT32_FAMILY}_crm.c \
	$(AT32_SDK_LIB)/drivers/src/${AT32_FAMILY}_acc.c \
	$(AT32_SDK_LIB)/drivers/src/${AT32_FAMILY}_exint.c \
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

flash: flash-atlink
