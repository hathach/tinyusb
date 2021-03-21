DEPS_SUBMODULES += hw/mcu/microchip

CONF_CPU_FREQUENCY ?= 48000000

CFLAGS += \
  -mthumb \
  -mabi=aapcs \
  -mlong-calls \
  -mcpu=cortex-m4 \
  -mfloat-abi=hard \
  -mfpu=fpv4-sp-d16 \
  -nostdlib -nostartfiles \
  -D__SAME54P20A__ \
  -DCONF_CPU_FREQUENCY=$(CONF_CPU_FREQUENCY) \
  -DCFG_TUSB_MCU=OPT_MCU_SAME5X \
  -DBOARD_NAME="\"Microchip SAM E54 Xplained Pro\""


# All source paths should be relative to the top level.
LD_FILE = hw/bsp/$(BOARD)/same54p20a_flash.ld

SRC_C += \
  src/portable/microchip/samd/dcd_samd.c \
  hw/mcu/microchip/same54/gcc/gcc/startup_same54.c \
  hw/mcu/microchip/same54/gcc/system_same54.c \
  hw/mcu/microchip/same54/hal/utils/src/utils_syscalls.c

INC += \
	$(TOP)/hw/mcu/microchip/same54/ \
	$(TOP)/hw/mcu/microchip/same54/config \
	$(TOP)/hw/mcu/microchip/same54/include \
	$(TOP)/hw/mcu/microchip/same54/hal/include \
	$(TOP)/hw/mcu/microchip/same54/hal/utils/include \
	$(TOP)/hw/mcu/microchip/same54/hpl/port \
	$(TOP)/hw/mcu/microchip/same54/hri \
	$(TOP)/hw/mcu/microchip/same54/CMSIS/Include

# For freeRTOS port source
FREERTOS_PORT = ARM_CM4F

# For flash-jlink target
JLINK_DEVICE = ATSAME54P20

# flash using edbg from https://github.com/ataradov/edbg
flash: $(BUILD)/$(PROJECT).bin
	edbg --verbose -t same54 -pv -f $<
