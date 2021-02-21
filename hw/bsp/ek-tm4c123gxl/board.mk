CFLAGS += \
  -flto \
  -mthumb \
  -mabi=aapcs \
  -mcpu=cortex-m4 \
  -mfloat-abi=hard \
  -mfpu=fpv4-sp-d16 \
  -nostdlib \
  -DCFG_TUSB_MCU=OPT_MCU_TM4C123 \
  -DTM4C123GH6PM
  
# lpc_types.h cause following errors
CFLAGS += -Wno-error=strict-prototypes

MCU_DIR = hw/mcu/ti/TM4C_Tiva-C/
CMSIS=$(TOP)/hw/mcu/ti/TM4C_Tiva-C/CMSIS/5.7.0/CMSIS/Include
TI_HDR=$(TOP)/hw/mcu/ti/TM4C_Tiva-C/Include/TM4C123/

# All source paths should be relative to the top level.

LD_FILE = hw/bsp/$(BOARD)/tm4c123.ld

LD_FILE = hw/bsp/$(BOARD)/tm4c123.ld

INC += \
     	$(CMSIS) \
	$(TI_HDR)

SRC_C += \
         $(MCU_DIR)/Source/GCC/tm4c123_startup.c 

# For TinyUSB port source
VENDOR = ti
CHIP_FAMILY = TM4C_Tiva-C

# For freeRTOS port source
FREERTOS_PORT = ARM_CM4

# For flash-jlink target
JLINK_DEVICE = LPC1769

# flash using jlink
flash: flash-jlink
