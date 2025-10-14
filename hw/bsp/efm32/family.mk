include $(TOP)/$(BOARD_PATH)/board.mk

CFLAGS += \
  -flto \
  -mthumb \
  -mcpu=cortex-m4 \
  -mfloat-abi=hard \
  -mfpu=fpv4-sp-d16 \
  -nostdlib -nostartfiles \
  -D__STARTUP_CLEAR_BSS \
  -D__START=main \
  -DCFG_TUSB_MCU=OPT_MCU_EFM32GG

CPU_CORE ?= cortex-m4

# EFM32_FAMILY should be set by board.mk (e.g. efm32gg12b)
SILABS_CMSIS = hw/mcu/silabs/cmsis-dfp-$(EFM32_FAMILY)/Device/SiliconLabs/$(shell echo $(EFM32_FAMILY) | tr a-z A-Z)

LDFLAGS_GCC += -specs=nosys.specs -specs=nano.specs

# All source paths should be relative to the top level.
LD_FILE = $(SILABS_CMSIS)/Source/GCC/$(EFM32_FAMILY).ld

SRC_C += \
  $(SILABS_CMSIS)/Source/system_$(EFM32_FAMILY).c \
	src/portable/synopsys/dwc2/dcd_dwc2.c \
	src/portable/synopsys/dwc2/hcd_dwc2.c \
	src/portable/synopsys/dwc2/dwc2_common.c \

SRC_S += \
  $(SILABS_CMSIS)/Source/GCC/startup_$(EFM32_FAMILY).S

INC += \
  $(TOP)/lib/CMSIS_5/CMSIS/Core/Include \
  $(TOP)/$(SILABS_CMSIS)/Include \
  $(TOP)/$(BOARD_PATH)

# For freeRTOS port source
FREERTOS_PORTABLE_SRC = $(FREERTOS_PORTABLE_PATH)/ARM_CM4F
