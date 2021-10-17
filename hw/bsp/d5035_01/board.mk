DEPS_SUBMODULES += hw/mcu/microchip
HWREV ?= 1

CFLAGS += \
  -mthumb \
  -mabi=aapcs \
  -mlong-calls \
  -mcpu=cortex-m4 \
  -mfloat-abi=hard \
  -mfpu=fpv4-sp-d16 \
  -nostdlib -nostartfiles \
  -D__SAME51J19A__ \
  -DCONF_CPU_FREQUENCY=80000000 \
  -DCONF_GCLK_USB_FREQUENCY=48000000 \
  -DCFG_TUSB_MCU=OPT_MCU_SAME5X \
  -DD5035_01=1 \
  -DBOARD_NAME="\"D5035-01\"" \
  -DSVC_Handler=SVCall_Handler \
  -DHWREV=$(HWREV)

# suppress warning caused by vendor mcu driver
CFLAGS += -Wno-error=cast-qual

# All source paths should be relative to the top level.
LD_FILE = hw/bsp/$(BOARD)/same51j19a_flash.ld

SRC_C += \
  src/portable/microchip/samd/dcd_samd.c \
  hw/mcu/microchip/same51/gcc/gcc/startup_same51.c \
  hw/mcu/microchip/same51/gcc/system_same51.c

ifdef SYSCALLS
ifneq ($(SYSCALLS),0)
  SRC_C += hw/mcu/microchip/same51/hal/utils/src/utils_syscalls.c
endif
endif

ifdef LOG
ifneq ($(LOG),0)
  SRC_C += hw/mcu/microchip/same51/hal/utils/src/utils_syscalls.c
endif
endif

INC += \
	$(TOP)/hw/mcu/microchip/same51/ \
	$(TOP)/hw/mcu/microchip/same51/config \
	$(TOP)/hw/mcu/microchip/same51/include \
	$(TOP)/hw/mcu/microchip/same51/hal/include \
	$(TOP)/hw/mcu/microchip/same51/hal/utils/include \
	$(TOP)/hw/mcu/microchip/same51/hpl/port \
	$(TOP)/hw/mcu/microchip/same51/hri \
	$(TOP)/hw/mcu/microchip/same51/CMSIS/Include

# For freeRTOS port source
FREERTOS_PORT = ARM_CM4F

# For flash-jlink target
JLINK_DEVICE = ATSAME51J19

# flash using jlink
flash: flash-jlink
