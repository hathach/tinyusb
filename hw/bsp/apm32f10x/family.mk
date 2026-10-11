APM32_FAMILY = apm32f10x
APM32_SDK = hw/mcu/geehy/APM32F10x_SDK/Libraries

include $(TOP)/$(BOARD_PATH)/board.mk

CPU_CORE ?= cortex-m3

CFLAGS += \
  -flto

ifeq ($(BOARD),apm32f103_dev_board)
CFLAGS += \
	-DCFG_TUSB_MCU=OPT_MCU_APM32F103

SRC_C += \
	src/portable/st/stm32_fsdev/dcd_stm32_fsdev.c \
	src/portable/st/stm32_fsdev/fsdev_common.c \
else ifeq ($(BOARD),apm32f107_dev_board)
CFLAGS += \
	-DCFG_TUSB_MCU=OPT_MCU_APM32F107

SRC_C += \
	src/portable/synopsys/dwc2/dcd_dwc2.c \
	src/portable/synopsys/dwc2/hcd_dwc2.c \
	src/portable/synopsys/dwc2/dwc2_common.c \
endif

LDFLAGS += \
	-flto --specs=nosys.specs -nostdlib -nostartfiles

SRC_C += \
	$(APM32_SDK)/APM32F10x_StdPeriphDriver/src/apm32f10x_gpio.c \
	$(APM32_SDK)/APM32F10x_StdPeriphDriver/src/apm32f10x_misc.c \
	$(APM32_SDK)/APM32F10x_StdPeriphDriver/src/apm32f10x_rcm.c \
	$(APM32_SDK)/APM32F10x_StdPeriphDriver/src/apm32f10x_usart.c \
	$(APM32_SDK)/Device/Geehy/APM32F10x/Source/system_apm32f10x.c

INC += \
	$(TOP)/$(BOARD_PATH) \
	$(TOP)/$(APM32_SDK)/APM32F10x_StdPeriphDriver/inc \
	$(TOP)/$(APM32_SDK)/CMSIS/Include \
	$(TOP)/$(APM32_SDK)/Device/Geehy/APM32F10x/Include

# F103: startup_apm32f103xb.S
# F107: startup_apm32f107xc.S
SRC_S += $(APM32_SDK)/Device/Geehy/APM32F10x/Source/gcc/startup_$(MCU_VARIANT).S

LD_FILE ?= $(APM32_SDK)/Device/Geehy/APM32F10x/Source/gcc/$(MCU_LINKER_NAME)_flash.ld

# For freeRTOS port source
FREERTOS_PORTABLE_SRC = $(FREERTOS_PORTABLE_PATH)/ARM_CM3

flash: flash-jlink
