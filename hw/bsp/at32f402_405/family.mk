AT32_FAMILY = at32f402_405
AT32_SDK_LIB = hw/mcu/artery/${AT32_FAMILY}/libraries

include $(TOP)/$(BOARD_PATH)/board.mk

CPU_CORE ?= cortex-m4

CFLAGS_GCC += \
  -flto

RHPORT_SPEED ?= OPT_MODE_FULL_SPEED OPT_MODE_FULL_SPEED
RHPORT_DEVICE ?= 0
RHPORT_HOST ?= 0

ifndef RHPORT_DEVICE_SPEED
ifeq ($(RHPORT_DEVICE), 0)
  RHPORT_DEVICE_SPEED = $(firstword $(RHPORT_SPEED))
else
  RHPORT_DEVICE_SPEED = $(lastword $(RHPORT_SPEED))
endif
endif

ifndef RHPORT_HOST_SPEED
ifeq ($(RHPORT_HOST), 0)
  RHPORT_HOST_SPEED = $(firstword $(RHPORT_SPEED))
else
  RHPORT_HOST_SPEED = $(lastword $(RHPORT_SPEED))
endif
endif

CFLAGS += \
    -DCFG_TUSB_MCU=OPT_MCU_AT32F402_405 \
	-DBOARD_TUD_RHPORT=${RHPORT_DEVICE} \
	-DBOARD_TUD_MAX_SPEED=${RHPORT_DEVICE_SPEED} \
	-DBOARD_TUH_RHPORT=${RHPORT_HOST} \
	-DBOARD_TUH_MAX_SPEED=${RHPORT_HOST_SPEED} \

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
