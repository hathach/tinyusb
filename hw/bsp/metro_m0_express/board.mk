CFLAGS = \
	-DCFG_TUSB_MCU=OPT_MCU_SAMD21 \
	-DCONF_DFLL_OVERWRITE_CALIBRATION=0 \
	-D__SAMD21G18A__ \
	-mthumb \
	-mabi=aapcs-linux \
	-mcpu=cortex-m0plus \
	-msoft-float \
	-mfloat-abi=soft

# All source paths should be relative to the top level.
LD_FILE = hw/bsp/metro_m0_express/samd21g18a_flash.ld

LD_FLAGS += -mthumb -mcpu=cortex-m0plus

SRC_C += \
	hw/mcu/microchip/samd/asf4/samd21/gcc/gcc/startup_samd21.c \
	hw/mcu/microchip/samd/asf4/samd21/gcc/system_samd21.c \
	hw/mcu/microchip/samd/asf4/samd21/hpl/gclk/hpl_gclk.c \
	hw/mcu/microchip/samd/asf4/samd21/hpl/pm/hpl_pm.c \
	hw/mcu/microchip/samd/asf4/samd21/hpl/sysctrl/hpl_sysctrl.c

INC += \
	-I$(TOP)/hw/mcu/microchip/samd/asf4/samd21/ \
	-I$(TOP)/hw/mcu/microchip/samd/asf4/samd21/config \
	-I$(TOP)/hw/mcu/microchip/samd/asf4/samd21/include \
	-I$(TOP)/hw/mcu/microchip/samd/asf4/samd21/hal/include \
	-I$(TOP)/hw/mcu/microchip/samd/asf4/samd21/hal/utils/include \
	-I$(TOP)/hw/mcu/microchip/samd/asf4/samd51/hpl/pm/ \
	-I$(TOP)/hw/mcu/microchip/samd/asf4/samd21/hpl/port \
	-I$(TOP)/hw/mcu/microchip/samd/asf4/samd21/hri \
	-I$(TOP)/hw/mcu/microchip/samd/asf4/samd21/CMSIS/Include

VENDOR = microchip
CHIP_FAMILY = samd21
