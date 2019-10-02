CFLAGS += \
  -mthumb \
  -mabi=aapcs \
  -mcpu=cortex-m4 \
  -mfloat-abi=hard \
  -mfpu=fpv4-sp-d16 \
  -DNRF52840_XXAA \
  -DCONFIG_GPIO_AS_PINRESET \
  -DCFG_TUSB_MCU=OPT_MCU_NRF5X

# nrfx issue undef _ARMCC_VERSION usage https://github.com/NordicSemiconductor/nrfx/issues/49
CFLAGS += -Wno-error=undef -Wno-error=unused-parameter

# due to tusb_hal_nrf_power_event
GCCVERSION = $(firstword $(subst ., ,$(shell arm-none-eabi-gcc -dumpversion)))
ifeq ($(shell expr $(GCCVERSION) \>= 8), 1)
CFLAGS += -Wno-error=cast-function-type
endif

# All source paths should be relative to the top level.
LD_FILE = hw/bsp/$(BOARD)/$(BOARD).ld

LDFLAGS += -L$(TOP)/hw/mcu/nordic/nrfx/mdk

SRC_C += \
	hw/mcu/nordic/nrfx/drivers/src/nrfx_power.c \
	hw/mcu/nordic/nrfx/mdk/system_nrf52840.c \

INC += \
	$(TOP)/hw/mcu/nordic/cmsis/Include \
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

# flash using Nordic nrfutil (pip2 install nrfutil)
# 	make BOARD=pca10059 SERIAL=/dev/ttyACM0 all flash
NRFUTIL = nrfutil

$(BUILD)/$(BOARD)-firmware.zip: $(BUILD)/$(BOARD)-firmware.hex
	$(NRFUTIL) pkg generate --hw-version 52 --sd-req 0x0000 --debug-mode --application $^ $@

flash: $(BUILD)/$(BOARD)-firmware.zip
	@:$(call check_defined, SERIAL, example: SERIAL=/dev/ttyACM0)
	$(NRFUTIL) dfu usb-serial --package $^ -p $(SERIAL) -b 115200
