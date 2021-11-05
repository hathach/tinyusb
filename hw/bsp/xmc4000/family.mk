UF2_FAMILY_ID = 0x00
MCU_DIR = hw/mcu/infineon/mtb-xmclib-cat3

DEPS_SUBMODULES += $(MCU_DIR)

include $(TOP)/$(BOARD_PATH)/board.mk

CFLAGS += \
  -flto \
  -mthumb \
  -mabi=aapcs \
  -mcpu=cortex-m4 \
  -mfloat-abi=hard \
  -mfpu=fpv4-sp-d16 \
  -nostdlib -nostartfiles \
  -DCFG_TUSB_MCU=OPT_MCU_XMC4000

# mcu driver cause following warnings
#CFLAGS += -Wno-error=shadow -Wno-error=cast-align

SKIP_NANOLIB = 1

SRC_C += \
	src/portable/synopsys/dwc2/dcd_dwc2.c \
	$(MCU_DIR)/Newlib/syscalls.c \
	$(MCU_DIR)/CMSIS/Infineon/COMPONENT_$(MCU_VARIANT)/Source/system_$(MCU_VARIANT).c \
	$(MCU_DIR)/XMCLib/src/xmc4_gpio.c \
	$(MCU_DIR)/XMCLib/src/xmc4_scu.c


SRC_S += $(MCU_DIR)/CMSIS/Infineon/COMPONENT_$(MCU_VARIANT)/Source/TOOLCHAIN_GCC_ARM/startup_$(MCU_VARIANT).S

INC += \
  $(TOP)/$(BOARD_PATH) \
	$(TOP)/$(MCU_DIR)/CMSIS/Core/Include \
	$(TOP)/$(MCU_DIR)/CMSIS/Infineon/COMPONENT_$(MCU_VARIANT)/Include \
	$(TOP)/$(MCU_DIR)/XMCLib/inc

# For freeRTOS port source
FREERTOS_PORT = ARM_CM4F
