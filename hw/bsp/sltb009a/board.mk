CFLAGS += \
  -flto \
  -mthumb \
  -mcpu=cortex-m4 \
  -mfloat-abi=hard \
  -mfpu=fpv4-sp-d16 \
  -nostdlib -nostartfiles \
  -D__STARTUP_CLEAR_BSS \
  -D__START=main \
  -DEFM32GG12B810F1024GM64 \
  -DCFG_TUSB_MCU=OPT_MCU_EFM32GG12 

# mcu driver cause following warnings
#CFLAGS += -Wno-error=unused-parameter

SILABS_FAMILY = efm32gg12b
SILABS_CMSIS = hw/mcu/silabs/cmsis-dfp-$(SILABS_FAMILY)/Device/SiliconLabs/$(shell echo $(SILABS_FAMILY) | tr a-z A-Z)

DEPS_SUBMODULES += hw/mcu/silabs/cmsis-dfp-$(SILABS_FAMILY)
DEPS_SUBMODULES += lib/CMSIS_5

# All source paths should be relative to the top level.
LD_FILE = $(SILABS_CMSIS)/Source/GCC/$(SILABS_FAMILY).ld

SRC_C += \
  $(SILABS_CMSIS)/Source/system_$(SILABS_FAMILY).c \
  src/portable/silabs/efm32/dcd_efm32.c

SRC_S += \
  $(SILABS_CMSIS)/Source/GCC/startup_$(SILABS_FAMILY).S

INC += \
  $(TOP)/lib/CMSIS_5/CMSIS/Core/Include \
  $(TOP)/$(SILABS_CMSIS)/Include \
  $(TOP)/hw/bsp/$(BOARD)

# For TinyUSB port source
VENDOR = silabs
CHIP_FAMILY = efm32

# For freeRTOS port source
FREERTOS_PORT = ARM_CM4

# For flash-jlink target
JLINK_DEVICE = EFM32GG12B810F1024

flash: flash-jlink
