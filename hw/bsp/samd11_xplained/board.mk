CFLAGS += \
  -mthumb \
  -mabi=aapcs-linux \
  -mcpu=cortex-m0plus \
  -nostdlib -nostartfiles \
  -D__SAMD11D14AM__ \
  -DCONF_DFLL_OVERWRITE_CALIBRATION=0 \
  -DOSC32K_OVERWRITE_CALIBRATION=0 \
  -DCFG_TUSB_MCU=OPT_MCU_SAMD11

# All source paths should be relative to the top level.
LD_FILE = hw/bsp/$(BOARD)/samd11d14am_flash.ld

SRC_C += \
	hw/mcu/microchip/samd11/gcc/gcc/startup_samd11.c \
	hw/mcu/microchip/samd11/gcc/system_samd11.c \
	hw/mcu/microchip/samd11/hpl/gclk/hpl_gclk.c \
	hw/mcu/microchip/samd11/hpl/pm/hpl_pm.c \
	hw/mcu/microchip/samd11/hpl/sysctrl/hpl_sysctrl.c \
	hw/mcu/microchip/samd11/hal/src/hal_atomic.c

INC += \
	$(TOP)/hw/mcu/microchip/samd11/ \
	$(TOP)/hw/mcu/microchip/samd11/config \
	$(TOP)/hw/mcu/microchip/samd11/include \
	$(TOP)/hw/mcu/microchip/samd11/hal/include \
	$(TOP)/hw/mcu/microchip/samd11/hal/utils/include \
	$(TOP)/hw/mcu/microchip/samd11/hpl/pm/ \
	$(TOP)/hw/mcu/microchip/samd11/hpl/port \
	$(TOP)/hw/mcu/microchip/samd11/hri \
	$(TOP)/hw/mcu/microchip/samd11/CMSIS/Include \
	$(TOP)/hw/mcu/microchip/samd11/CMSIS/Core/Include

# For TinyUSB port source
VENDOR = microchip
CHIP_FAMILY = samd

# For freeRTOS port source
FREERTOS_PORT = ARM_CM0

# For flash-jlink target
JLINK_DEVICE = ATSAMD11D14

# flash using edbg
flash: $(BUILD)/$(BOARD)-firmware.bin
	edbg -b -t samd11 -e -pv -f $<
