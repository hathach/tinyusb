APM32_FAMILY = apm32f0xx
APM32_SDK = hw/mcu/geehy/APM32F0xx_SDK_V1.8.6/Libraries

include $(TOP)/$(BOARD_PATH)/board.mk

CPU_CORE ?= cortex-m0plus

CFLAGS += \
  -flto

CFLAGS += \
	-DCFG_TUSB_MCU=OPT_MCU_APM32F0XX

LDFLAGS += \
	-flto --specs=nosys.specs -nostdlib -nostartfiles

SRC_C += \
	src/portable/st/stm32_fsdev/dcd_stm32_fsdev.c \
	src/portable/st/stm32_fsdev/fsdev_common.c \
	$(APM32_SDK)/APM32F0xx_StdPeriphDriver/src/apm32f0xx_gpio.c \
	$(APM32_SDK)/APM32F0xx_StdPeriphDriver/src/apm32f0xx_misc.c \
	$(APM32_SDK)/APM32F0xx_StdPeriphDriver/src/apm32f0xx_rcm.c \
	$(APM32_SDK)/APM32F0xx_StdPeriphDriver/src/apm32f0xx_crs.c \
	$(APM32_SDK)/Device/Geehy/APM32F0xx/Source/system_apm32f0xx.c

INC += \
	$(TOP)/$(BOARD_PATH) \
	$(TOP)/$(APM32_SDK)/APM32F0xx_StdPeriphDriver/inc \
	$(TOP)/$(APM32_SDK)/CMSIS/Include \
	$(TOP)/$(APM32_SDK)/Device/Geehy/APM32F0xx/Include

SRC_S += $(APM32_SDK)/Device/Geehy/APM32F0xx/Source/gcc/startup_apm32f072.S

LD_FILE ?= $(APM32_SDK)/Device/Geehy/APM32F0xx/Source/gcc/gcc_${MCU_LINKER_NAME}.ld

# For freeRTOS port source
FREERTOS_PORTABLE_SRC = $(FREERTOS_PORTABLE_PATH)/ARM_CM0

flash: flash-jlink
