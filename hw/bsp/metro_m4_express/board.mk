CFLAGS += \
  -flto \
  -mthumb \
  -mabi=aapcs-linux \
  -mcpu=cortex-m4 \
  -mfloat-abi=hard \
  -mfpu=fpv4-sp-d16 \
  -nostdlib -nostartfiles \
  -D__SAMD51J19A__ \
  -DCFG_TUSB_MCU=OPT_MCU_SAMD51

CFLAGS += -Wno-error=undef

# All source paths should be relative to the top level.
LD_FILE = hw/bsp/$(BOARD)/samd51g19a_flash.ld

SRC_C += \
	hw/mcu/microchip/samd/asf4/samd51/gcc/gcc/startup_samd51.c \
	hw/mcu/microchip/samd/asf4/samd51/gcc/system_samd51.c \
	hw/mcu/microchip/samd/asf4/samd51/hpl/gclk/hpl_gclk.c \
	hw/mcu/microchip/samd/asf4/samd51/hpl/mclk/hpl_mclk.c \
	hw/mcu/microchip/samd/asf4/samd51/hpl/osc32kctrl/hpl_osc32kctrl.c \
	hw/mcu/microchip/samd/asf4/samd51/hpl/oscctrl/hpl_oscctrl.c \
	hw/mcu/microchip/samd/asf4/samd51/hal/src/hal_atomic.c

INC += \
	$(TOP)/hw/mcu/microchip/samd/asf4/samd51/ \
	$(TOP)/hw/mcu/microchip/samd/asf4/samd51/config \
	$(TOP)/hw/mcu/microchip/samd/asf4/samd51/include \
	$(TOP)/hw/mcu/microchip/samd/asf4/samd51/hal/include \
	$(TOP)/hw/mcu/microchip/samd/asf4/samd51/hal/utils/include \
	$(TOP)/hw/mcu/microchip/samd/asf4/samd51/hpl/port \
	$(TOP)/hw/mcu/microchip/samd/asf4/samd51/hri \
	$(TOP)/hw/mcu/microchip/samd/asf4/samd51/CMSIS/Include

# For TinyUSB port source
VENDOR = microchip
CHIP_FAMILY = samd

# For freeRTOS port source
FREERTOS_PORT = ARM_CM4F

# For flash-jlink target
JLINK_DEVICE = ATSAMD51J19
JLINK_IF = swd

# flash using jlink
flash: flash-jlink
