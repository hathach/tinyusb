UF2_FAMILY_ID = 0x0
SDK_DIR = hw/mcu/mindmotion/mm32sdk
DEPS_SUBMODULES += lib/CMSIS_5 $(SDK_DIR)

include $(TOP)/$(BOARD_PATH)/board.mk
CPU_CORE ?= cortex-m3

CFLAGS += \
  -flto \
  -nostdlib -nostartfiles \
  -DCFG_TUSB_MCU=OPT_MCU_MM32F327X

# suppress warning caused by vendor mcu driver
CFLAGS += -Wno-error=unused-parameter -Wno-error=maybe-uninitialized -Wno-error=cast-qual

SRC_C += \
	src/portable/mindmotion/mm32/dcd_mm32f327x_otg.c \
	$(SDK_DIR)/mm32f327x/MM32F327x/Source/system_mm32f327x.c \
	$(SDK_DIR)/mm32f327x/MM32F327x/HAL_Lib/Src/hal_gpio.c \
	$(SDK_DIR)/mm32f327x/MM32F327x/HAL_Lib/Src/hal_rcc.c \
	$(SDK_DIR)/mm32f327x/MM32F327x/HAL_Lib/Src/hal_uart.c \

INC += \
	$(TOP)/$(BOARD_PATH) \
	$(TOP)/lib/CMSIS_5/CMSIS/Core/Include \
	$(TOP)/$(SDK_DIR)/mm32f327x/MM32F327x/Include \
	$(TOP)/$(SDK_DIR)/mm32f327x/MM32F327x/HAL_Lib/Inc

# flash target using on-board
flash: flash-jlink
