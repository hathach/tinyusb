DEPS_SUBMODULES += hw/mcu/ti

CFLAGS += \
  -flto \
  -mthumb \
  -mabi=aapcs \
  -mcpu=cortex-m4 \
  -mfloat-abi=hard \
  -mfpu=fpv4-sp-d16 \
  -DCFG_TUSB_MCU=OPT_MCU_TM4C123 \
  -uvectors \
  -DTM4C123GH6PM
  
# mcu driver cause following warnings
CFLAGS += -Wno-error=strict-prototypes -Wno-error=cast-qual

MCU_DIR=hw/mcu/ti/tm4c123xx/

# All source paths should be relative to the top level.
LD_FILE = $(BOARD_PATH)/tm4c123.ld

INC += \
	$(TOP)/$(MCU_DIR)/CMSIS/5.7.0/CMSIS/Include \
	$(TOP)/$(MCU_DIR)/Include/TM4C123 \
	$(TOP)/hw/bsp

SRC_C += \
	src/portable/mentor/musb/dcd_musb.c \
	$(MCU_DIR)/Source/system_TM4C123.c \
	$(MCU_DIR)/Source/GCC/tm4c123_startup.c

# For freeRTOS port source
FREERTOS_PORT = ARM_CM4F

# For flash-jlink target
JLINK_DEVICE = TM4C123GH6PM

# flash using openocd
OPENOCD_OPTION = -f board/ti_ek-tm4c123gxl.cfg
flash: flash-openocd
