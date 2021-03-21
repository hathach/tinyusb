
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
  
# lpc_types.h cause following errors
CFLAGS += -Wno-error=strict-prototypes

MCU_DIR=hw/mcu/ti/tm4c123xx/

CMSIS=$(TOP)/hw/mcu/ti/tm4c123xx/CMSIS/5.7.0/CMSIS/Include

TI_HDR=$(TOP)/hw/mcu/ti/tm4c123xx/Include/TM4C123/

# All source paths should be relative to the top level.

LD_FILE = hw/bsp/$(BOARD)/tm4c123.ld

INC += \
     	$(CMSIS) \
      $(TI_HDR) \
      $(TOP)/hw/bsp 


SRC_C += \
         $(MCU_DIR)/Source/system_TM4C123.c \
         $(MCU_DIR)/Source/GCC/tm4c123_startup.c 

# For TinyUSB port source
VENDOR = ti
CHIP_FAMILY = tm4c123xx

# For freeRTOS port source
FREERTOS_PORT = ARM_CM4F

# For flash-jlink target
JLINK_DEVICE = LPC1769

# flash using jlink
flash: $(BUILD)/$(PROJECT).elf
	openocd -f board/ti_ek-tm4c123gxl.cfg  -c "program $< verify reset exit"
