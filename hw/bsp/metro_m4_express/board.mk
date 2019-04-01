CFLAGS += \
	-DCFG_TUSB_MCU=OPT_MCU_SAMD51 \
	-D__SAMD51J19A__ \
	-mthumb \
	-mabi=aapcs-linux \
	-mcpu=cortex-m4 \
	-mfloat-abi=hard \
	-mfpu=fpv4-sp-d16 \
	-nostdlib -nostartfiles

CFLAGS += -Wno-error=undef

# All source paths should be relative to the top level.
LD_FILE = hw/bsp/metro_m4_express/samd51g19a_flash.ld

LDFLAGS += -mthumb -mcpu=cortex-m4

SRC_C += \
	hw/mcu/microchip/samd/asf4/samd51/gcc/gcc/startup_samd51.c \
	hw/mcu/microchip/samd/asf4/samd51/gcc/system_samd51.c \
	hw/mcu/microchip/samd/asf4/samd51/hpl/gclk/hpl_gclk.c \
	hw/mcu/microchip/samd/asf4/samd51/hpl/mclk/hpl_mclk.c \
	hw/mcu/microchip/samd/asf4/samd51/hpl/osc32kctrl/hpl_osc32kctrl.c \
	hw/mcu/microchip/samd/asf4/samd51/hpl/oscctrl/hpl_oscctrl.c \
	hw/mcu/microchip/samd/asf4/samd51/hal/src/hal_atomic.c

INC += \
	-I$(TOP)/hw/mcu/microchip/samd/asf4/samd51/ \
	-I$(TOP)/hw/mcu/microchip/samd/asf4/samd51/config \
	-I$(TOP)/hw/mcu/microchip/samd/asf4/samd51/include \
	-I$(TOP)/hw/mcu/microchip/samd/asf4/samd51/hal/include \
	-I$(TOP)/hw/mcu/microchip/samd/asf4/samd51/hal/utils/include \
	-I$(TOP)/hw/mcu/microchip/samd/asf4/samd51/hpl/port \
	-I$(TOP)/hw/mcu/microchip/samd/asf4/samd51/hri \
	-I$(TOP)/hw/mcu/microchip/samd/asf4/samd51/CMSIS/Include

VENDOR = microchip
CHIP_FAMILY = samd51

JLINK_DEVICE = ATSAMD51J19

# flash using jlink
flash: flash-jlink
