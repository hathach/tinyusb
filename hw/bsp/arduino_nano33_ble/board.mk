CFLAGS += \
  -flto \
  -mthumb \
  -mabi=aapcs \
  -mcpu=cortex-m4 \
  -mfloat-abi=hard \
  -mfpu=fpv4-sp-d16 \
  -DCFG_TUSB_MCU=OPT_MCU_NRF5X \
  -DNRF52840_XXAA \
  -DCONFIG_GPIO_AS_PINRESET

# nrfx issue undef _ARMCC_VERSION usage https://github.com/NordicSemiconductor/nrfx/issues/49
CFLAGS += -Wno-error=undef -Wno-error=unused-parameter

# due to tusb_hal_nrf_power_event
GCCVERSION = $(firstword $(subst ., ,$(shell arm-none-eabi-gcc -dumpversion)))
ifeq ($(shell expr $(GCCVERSION) \>= 8), 1)
CFLAGS += -Wno-error=cast-function-type
endif

# All source paths should be relative to the top level.
#LD_FILE = hw/bsp/$(BOARD)/linker_script.ld
LD_FILE = hw/bsp/$(BOARD)/arduino_nano33_ble.ld

LDFLAGS += -L$(TOP)/hw/mcu/nordic/nrfx/mdk

SRC_C += \
  hw/mcu/nordic/nrfx/drivers/src/nrfx_power.c \
  hw/mcu/nordic/nrfx/drivers/src/nrfx_uarte.c \
  hw/mcu/nordic/nrfx/mdk/system_nrf52840.c

INC += \
  $(TOP)/lib/CMSIS_4/CMSIS/Include \
  $(TOP)/hw/mcu/nordic \
  $(TOP)/hw/mcu/nordic/nrfx \
  $(TOP)/hw/mcu/nordic/nrfx/mdk \
  $(TOP)/hw/mcu/nordic/nrfx/hal \
  $(TOP)/hw/mcu/nordic/nrfx/drivers/include \
  $(TOP)/hw/mcu/nordic/nrfx/drivers/src \

SRC_S += hw/mcu/nordic/nrfx/mdk/gcc_startup_nrf52840.S

ASFLAGS += -D__HEAP_SIZE=0

# For TinyUSB port source
VENDOR = nordic
CHIP_FAMILY = nrf5x

# For freeRTOS port source
FREERTOS_PORT = ARM_CM4F

# For flash-jlink target
JLINK_DEVICE = nRF52840_xxAA
JLINK_IF = swd

# flash using bossac (as part of Nano33 BSP tools)
# can be found in arduino15/packages/arduino/tools/bossac/
# Add it to your PATH or change BOSSAC variable to match your installation
BOSSAC = bossac

flash: $(BUILD)/$(BOARD)-firmware.bin
	@:$(call check_defined, SERIAL, example: SERIAL=/dev/ttyACM0)
	$(BOSSAC) --port=$(SERIAL) -U -i -e -w $^ -R
